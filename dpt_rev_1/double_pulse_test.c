// double_pulse_test.c
// 
// Double Pulse Test — TMS320F2837xD (CPU1)
//
// five sections right now:
//  - starts with initial burst on GPIO0 [ePWM1A] to charge the bootstrap caps. 
//    next, goes into actual DPT with GPIO2 [ePWM2A]. 
//  - deadtime
//  - pulse on
//  - pulse off [repeat for configured num of cycles]
//  - finished
// timing can be configured below. 
//
// gpio35 is set up to be a "timing probe" -- basically shows when the sequence has begun/is in process
// without having to probe all of the other gpios directly which is nice
//

#include "driverlib.h"
#include "device.h"
#include "board.h"

//THESE ARE THE VALUES TO EDIT -- all timing is in us// 
#define DPT_BOOT_CHARGE_US  20U // time to charge bootstrap cap 
#define DPT_BOOT_GAP_US     3U  // time after first pulse before DPT
#define DPT_PULSE_US        5U  // first pulse
#define DPT_DEADTIME_US     5U  // off-time
#define DPT_INTER_CYCLE_US  50U // time between cycles
// #define DPT_PULSE2_US       2   // second pulse
// #define DPT_SINGLE_SHOT       1   // 1 for one go-around, 0 will repeat forever

#define DPT_NUM_PULSES      5U
#define DPT_NUM_CYCLES      0U   // 0 goes on forever. 1,2,3, etc. gives finite number of cycles


// CONSTANTS--
// ePWM timer counts at SYSCLK/1 (200 MHz).
// 1 µs = 200 counts.  Max is roughly 327 us for 16-bit TBPRD.
// if need longer than 327 us, increase EPWM_CLOCK_DIVIDER in initEPWM() functions
// and change DPT_SYSCLK_MHZ to reflect the divided clock.
 
#define DPT_SYSCLK_MHZ_INT      (DEVICE_SYSCLK_FREQ / 1000000UL)
#define US_TO_TBCNT_PP(us)      ((us) * DPT_SYSCLK_MHZ_INT / 2U)
// #define US_TO_TBCNT_PP(us)      ((us) * DPT_SYSCLK_MHZ_INT)
// trying to fix the above to be (sysclk_mhz_init/2) so that it matches epwmclk

//checks
#if US_TO_TBCNT_PP(DPT_BOOT_CHARGE_US) > 65535UL
#error "DPT_BOOT_CHARGE_US exceeds 16-bit TBPRD. Add clock prescaler."
#endif

#if US_TO_TBCNT_PP(DPT_PULSE1_US) > 65535UL
#error "DPT_PULSE1_US exceeds 16-bit TBPRD. Add clock prescaler."
#endif

#if DPT_NUM_PULSES < 1U
#error "DPT_NUM_PULSES must be at least 1."
#endif

#define DPT_SYSCLK_MHZ          ((float)DPT_SYSCLK_MHZ_INT)
#define US_TO_TBCNT(us)         ((uint16_t)((us) * DPT_SYSCLK_MHZ))

// scope probe to track all of the pulses
#ifndef DPT_PROBE_GPIO
#define DPT_PROBE_GPIO   35U
#endif
#define PROBE_HIGH()     GPIO_writePin(DPT_PROBE_GPIO, 1)
#define PROBE_LOW()      GPIO_writePin(DPT_PROBE_GPIO, 0)
#define PROBE_TOGGLE()   GPIO_togglePin(DPT_PROBE_GPIO)

// EPWM2 + SW-FORCE HELPERS
// Bootstrap pin is on ePWM2A, and driven w SW force. Uses same pattern as ePWM1A.  
// These are the only ways pins are touched after init

#define BOOT_PIN_HIGH() \
do { \
    EPWM_setActionQualifierSWAction(myEPWM2_BASE, (EPWM_ActionQualifierOutputModule)EPWM_AQ_OUTPUT_A, (EPWM_ActionQualifierOutput)EPWM_AQ_SW_OUTPUT_HIGH); \
    EPWM_forceActionQualifierSWAction(myEPWM2_BASE, (EPWM_ActionQualifierOutputModule)EPWM_AQ_OUTPUT_A); \
} while(0)

#define BOOT_PIN_LOW() \
do { \
    EPWM_setActionQualifierSWAction(myEPWM2_BASE, (EPWM_ActionQualifierOutputModule)EPWM_AQ_OUTPUT_A, (EPWM_ActionQualifierOutput)EPWM_AQ_SW_OUTPUT_LOW); \
    EPWM_forceActionQualifierSWAction(myEPWM2_BASE, (EPWM_ActionQualifierOutputModule)EPWM_AQ_OUTPUT_A); \
} while(0)

// GATE SW-FORCE HELPERS

#define GATE_HIGH() \
do { \
    EPWM_setActionQualifierSWAction(myEPWM1_BASE, (EPWM_ActionQualifierOutputModule)EPWM_AQ_OUTPUT_A, (EPWM_ActionQualifierOutput)EPWM_AQ_SW_OUTPUT_HIGH); \
    EPWM_forceActionQualifierSWAction(myEPWM1_BASE, (EPWM_ActionQualifierOutputModule)EPWM_AQ_OUTPUT_A); \
} while(0)

#define GATE_LOW() \
do { \
    EPWM_setActionQualifierSWAction(myEPWM1_BASE, (EPWM_ActionQualifierOutputModule)EPWM_AQ_OUTPUT_A, (EPWM_ActionQualifierOutput)EPWM_AQ_SW_OUTPUT_LOW); \
    EPWM_forceActionQualifierSWAction(myEPWM1_BASE, (EPWM_ActionQualifierOutputModule)EPWM_AQ_OUTPUT_A); \
} while(0)

// TIMER HELPER: load ePWM1 period and restart counter  
// ePWM1 is the timing master. Its period-match interrupt drives all phases.
// ePWM2 has no interrupt, it is only force-set/cleared by the ePWM1 ISR. 
#define SET_PHASE_DURATION(us) \
    EPWM_setTimeBasePeriod(myEPWM1_BASE, US_TO_TBCNT(us)); \
    EPWM_setTimeBaseCounter(myEPWM1_BASE, 0U);              \
    EPWM_setTimeBaseCounterMode(myEPWM1_BASE, EPWM_COUNTER_MODE_UP)

// STATE MACHINE  
// DPT_BOOT_CHARGE -- timer is BOOT_CHARGE_US
// DPT_BOOT_GAP -- timer is BOOT_GAP_US
// DPT_PULSE1 -- timer is PULSE1_US
// DPT_DEADTIME -- timer is DEADTIME_US
// DPT_PULSE2 -- timer is PULSE2_US
// DPT_DONE -- done counting! will either be done or repeat, depending on settings

typedef enum {
    DPT_IDLE        = 0,
    DPT_BOOT_CHARGE = 1,
    DPT_BOOT_GAP    = 2,
    DPT_PULSE       = 3,
    DPT_DEADTIME    = 4,
    DPT_DONE        = 5
} DPT_State;

volatile DPT_State g_dptState = DPT_IDLE;
volatile uint16_t  g_pulseIdx  = 0U;   // 0-based index of current pulse   
volatile uint16_t  g_cycleCount = 0U;  // number of completed full cycles   
 

// timestamps for timing analysis (again, all in microseconds) 
// add to ccs -> view -> expressions
//      (g_cycBootEnd - g_cycBootStart) / 200.0         -> bootstrap charge time
//      (g_cycP1Start - g_cycBootEnd)   / 200.0         -> bootstrap gap
//      (g_cycPulseEnd[n] - g_cycPulseStart[n]) / 200.0 -> pulse gap, depending on which n-pulse

// todo -- figure out how to best work with debug mode
// another todo -- remind self where to include u for the integers (and float decimals)
// (up to this point, just doing what allows the code to compile/flash properly)
volatile uint32_t g_cycBootStart = 0;
volatile uint32_t g_cycBootEnd   = 0;
volatile uint32_t g_cycPulseStart[DPT_NUM_PULSES]   = {0U};
volatile uint32_t g_cycPulseEnd[DPT_NUM_PULSES]     = {0U};

// declarations
static void initEPWM1(void);
static void initEPWM2(void);
static void initProbeGPIO(void);
static void initCycleCounter(void);
static void armSequence(void);
static inline uint32_t readCycleCounter(void);
__interrupt void epwm1ISR(void);

//main lüp :D
void main(void)
{
    Device_init();
    Device_initGPIO();

    Interrupt_initModule();
    Interrupt_initVectorTable();

    // only ePWM1 deals with interrupts, ePWM2 follows to ePWM1
    Interrupt_register(INT_EPWM1, &epwm1ISR);

    Board_init();         

    initProbeGPIO();
    initCycleCounter();

    // need to init PWM2 first so that it's low before PWM1 starts
    initEPWM2();
    initEPWM1();

    Interrupt_enable(INT_EPWM1);
    EINT;
    ERTM;

    // start the process
    armSequence();
    for(;;)
    {
        if(g_dptState == DPT_DONE)
        {
            // check if we're actually done, or need to move onto the next cycle
            // [0 means run forever]
            if((DPT_NUM_CYCLES != 0U) && (g_cycleCount >= DPT_NUM_CYCLES))
            {
                // stop running 
                ESTOP0;
            }
            else
            {
                // move to the next cycle
                DEVICE_DELAY_US(DPT_INTER_CYCLE_US);
                armSequence();
            }
        }
    }
}

static void armSequence(void)
{
    // init to low
    GATE_LOW();
    BOOT_PIN_LOW();

    PROBE_HIGH();                           // triggers on the scope
    g_cycBootStart = readCycleCounter();    // ----
    
    g_pulseIdx = 0U;
    g_dptState = DPT_BOOT_CHARGE;

    // start it up
    BOOT_PIN_HIGH();
    SET_PHASE_DURATION(DPT_BOOT_CHARGE_US);
}
// ePWM1 ISR will drive all of the different parts of the program. 
// it stops the counter, does its action, records timestamp, sets up the next phase, and resets ctr
// as mentioned b4, ePWM2 is controlled entirely here w BOOT_PIN_HIGH/LOW

__interrupt void epwm1ISR(void)
{
    // freezes counter until ready for next state
    EPWM_setTimeBaseCounterMode(myEPWM1_BASE, EPWM_COUNTER_MODE_STOP_FREEZE);

    switch(g_dptState)
    {
        // stops the charging bootstrap pulse
        case DPT_BOOT_CHARGE:
            g_cycBootEnd = readCycleCounter();
            PROBE_TOGGLE();                  
            BOOT_PIN_LOW();                

            g_dptState = DPT_BOOT_GAP;
            SET_PHASE_DURATION(DPT_BOOT_GAP_US);
            break;

        // ready for actual dpt
        case DPT_BOOT_GAP:
            g_cycPulseStart[g_pulseIdx] = readCycleCounter();
            PROBE_TOGGLE();                  
            GATE_HIGH();                    

            g_dptState = DPT_PULSE;
            SET_PHASE_DURATION(DPT_PULSE_US);
            break;

        // dpt section (on)
        case DPT_PULSE:
            g_cycPulseEnd[g_pulseIdx] = readCycleCounter();
            PROBE_TOGGLE();    
            GATE_LOW();
 
            if(g_pulseIdx < (DPT_NUM_PULSES - 1U))
            {
                // update the pulse number we're on and move onto the deadtime
                g_pulseIdx++;
                g_dptState = DPT_DEADTIME;
                SET_PHASE_DURATION(DPT_DEADTIME_US);
            }
            else
            {
                // done with the entire cycle, reset everything and finish
                PROBE_LOW();                
                g_cycleCount++;
                g_dptState = DPT_DONE;
            }
            break;
    
        // dpt section (off)
        case DPT_DEADTIME:
            g_cycPulseStart[g_pulseIdx] = readCycleCounter();
            PROBE_TOGGLE();          
            GATE_HIGH();
 
            g_dptState = DPT_PULSE;
            SET_PHASE_DURATION(DPT_PULSE_US);
            break;
        

        default:
            break;
    }

    EPWM_clearEventTriggerInterruptFlag(myEPWM1_BASE);
    Interrupt_clearACKGroup(INTERRUPT_ACK_GROUP3);
}
// initEPWM1, in charge of state transitions.
// output controlled by SW force, counter starts frozen. refer to armSequence()

static void initEPWM1(void)
{
    SysCtl_disablePeripheral(SYSCTL_PERIPH_CLK_TBCLKSYNC);

    EPWM_setClockPrescaler(myEPWM1_BASE,
                           EPWM_CLOCK_DIVIDER_1,
                           EPWM_HSCLOCK_DIVIDER_1);

    EPWM_setTimeBaseCounterMode(myEPWM1_BASE, EPWM_COUNTER_MODE_STOP_FREEZE);
    EPWM_setTimeBaseCounter(myEPWM1_BASE, 0U);
    EPWM_setTimeBasePeriod(myEPWM1_BASE, US_TO_TBCNT(DPT_BOOT_CHARGE_US));
    EPWM_setPeriodLoadMode(myEPWM1_BASE, EPWM_PERIOD_DIRECT_LOAD);

    // disables AQ, since we're using SW
    EPWM_setActionQualifierAction(myEPWM1_BASE, EPWM_AQ_OUTPUT_A,
        EPWM_AQ_OUTPUT_NO_CHANGE, EPWM_AQ_OUTPUT_ON_TIMEBASE_UP_CMPA);
    EPWM_setActionQualifierAction(myEPWM1_BASE, EPWM_AQ_OUTPUT_A,
        EPWM_AQ_OUTPUT_NO_CHANGE, EPWM_AQ_OUTPUT_ON_TIMEBASE_DOWN_CMPA);
    EPWM_setActionQualifierAction(myEPWM1_BASE, EPWM_AQ_OUTPUT_A,
        EPWM_AQ_OUTPUT_NO_CHANGE, EPWM_AQ_OUTPUT_ON_TIMEBASE_ZERO);
    EPWM_setActionQualifierAction(myEPWM1_BASE, EPWM_AQ_OUTPUT_A,
        EPWM_AQ_OUTPUT_NO_CHANGE, EPWM_AQ_OUTPUT_ON_TIMEBASE_PERIOD);

    // starting low to stay safe
    GATE_LOW();

    // disabling deadband, as timing is controlled in SW                    
    EPWM_disableDeadBandControlShadowLoadMode(myEPWM1_BASE);

    // interrupt on period match, every cycle
    EPWM_setInterruptSource(myEPWM1_BASE, EPWM_INT_TBCTR_PERIOD);
    EPWM_setInterruptEventCount(myEPWM1_BASE, 1U);
    EPWM_enableInterrupt(myEPWM1_BASE);
    EPWM_clearEventTriggerInterruptFlag(myEPWM1_BASE);

    SysCtl_enablePeripheral(SYSCTL_PERIPH_CLK_TBCLKSYNC);
}

// ePWM2 is used for the driver that will be used to charge the bootstrap, not because it needs 
// an interrupt or even repetition, but because the pin has a stronger drive of sorts.
static void initEPWM2(void)
{
    // ePWM2 shares the TB sync disable with ePWM1 — only call once    
    SysCtl_disablePeripheral(SYSCTL_PERIPH_CLK_TBCLKSYNC);

    EPWM_setClockPrescaler(myEPWM2_BASE,
                           EPWM_CLOCK_DIVIDER_1,
                           EPWM_HSCLOCK_DIVIDER_1);

    /* Counter runs free at max period — never self-interrupts          */
    EPWM_setTimeBasePeriod(myEPWM2_BASE, 0xFFFFU);
    EPWM_setPeriodLoadMode(myEPWM2_BASE, EPWM_PERIOD_DIRECT_LOAD);
    EPWM_setTimeBaseCounter(myEPWM2_BASE, 0U);
    EPWM_setTimeBaseCounterMode(myEPWM2_BASE, EPWM_COUNTER_MODE_UP);

    /* Disable all automatic AQ events on ePWM2A                       */
    EPWM_setActionQualifierAction(myEPWM2_BASE, EPWM_AQ_OUTPUT_A,
        EPWM_AQ_OUTPUT_NO_CHANGE, EPWM_AQ_OUTPUT_ON_TIMEBASE_UP_CMPA);
    EPWM_setActionQualifierAction(myEPWM2_BASE, EPWM_AQ_OUTPUT_A,
        EPWM_AQ_OUTPUT_NO_CHANGE, EPWM_AQ_OUTPUT_ON_TIMEBASE_DOWN_CMPA);
    EPWM_setActionQualifierAction(myEPWM2_BASE, EPWM_AQ_OUTPUT_A,
        EPWM_AQ_OUTPUT_NO_CHANGE, EPWM_AQ_OUTPUT_ON_TIMEBASE_ZERO);
    EPWM_setActionQualifierAction(myEPWM2_BASE, EPWM_AQ_OUTPUT_A,
        EPWM_AQ_OUTPUT_NO_CHANGE, EPWM_AQ_OUTPUT_ON_TIMEBASE_PERIOD);

    /* Safe initial state: bootstrap LOW                                */
    BOOT_PIN_LOW();

    // disabling pwm2 interrupts since we don't use them
    EPWM_disableInterrupt(myEPWM2_BASE);

    SysCtl_enablePeripheral(SYSCTL_PERIPH_CLK_TBCLKSYNC);
}

// init probe gpio
static void initProbeGPIO(void)
{
    GPIO_setPadConfig(DPT_PROBE_GPIO, GPIO_PIN_TYPE_STD);
    GPIO_setDirectionMode(DPT_PROBE_GPIO, GPIO_DIR_MODE_OUT);
    GPIO_setMasterCore(DPT_PROBE_GPIO, GPIO_CORE_CPU1);
    GPIO_setQualificationMode(DPT_PROBE_GPIO, GPIO_QUAL_SYNC);
    GPIO_writePin(DPT_PROBE_GPIO, 0);
}

// CPU Timer2 free-runs at SYSCLK (200 MHz), 1 count = 5 ns.
static void initCycleCounter(void)
{
    CPUTimer_stopTimer(CPUTIMER2_BASE);
    CPUTimer_setPreScaler(CPUTIMER2_BASE, 0U);
    CPUTimer_setPeriod(CPUTIMER2_BASE, 0xFFFFFFFFUL);
    CPUTimer_reloadTimerCounter(CPUTIMER2_BASE);
    CPUTimer_resumeTimer(CPUTIMER2_BASE);
}

static inline uint32_t readCycleCounter(void)
{
    return (uint32_t)(0xFFFFFFFFUL - CPUTimer_getTimerCount(CPUTIMER2_BASE));
}

//  all things together as a happy little reference:
//
//  first burst width       -  DPT_BOOT_CHARGE_US
//  gap before dpt          -  DPT_BOOT_GAP_US
//  pulse 1 width           -  DPT_PULSE1_US
//  off-time                -  DPT_DEADTIME_US
//  pulse 2 width           -  DPT_PULSE2_US
//  one vs repeated trial   -  DPT_SINGLE_SHOT
//  
//  pin for low-side gate   - myEPWM2_BASE / myEPWM2_GPIO in board.h
//  pin for main dpt gate   - myEPWM1_BASE / myEPWM1_GPIO in board.h
//  scope probe pin         - DPT_PROBE_GPIO (default 35)
//  
//  CCS Expressions for timing analysis (again, all in microseconds):
//      (g_cycBootEnd - g_cycBootStart) / 200.0   → bootstrap charge time
//      (g_cycP1Start - g_cycBootEnd)   / 200.0   → bootstrap gap
//      (g_cycP1End   - g_cycP1Start)   / 200.0   → pulse 1
//      (g_cycP2Start - g_cycP1End)     / 200.0   → dead-time
//      (g_cycP2End   - g_cycP2Start)   / 200.0   → pulse 2
