# micrOS-328

A bare-metal, preemptive, hard real-time operating system (RTOS) designed and implemented from scratch in C and AVR assembly for the standalone **ATmega328P** DIP-28 microcontroller (16 MHz external crystal).

Built with **zero dynamic memory allocation (`malloc`/`free`)**, **zero third-party kernel libraries**, and customized low-level hardware memory protection mechanisms.

---

## Key Architectural Features

- **Custom Assembly Context Switcher (`src/context_switch.S`):**
  - Manually pushes and pops the entire AVR CPU state: all 32 general-purpose working registers (`r0`–`r31`), the Status Register (`SREG`), and the Program Counter (`PC`).
  - Directly manipulates hardware Stack Pointer registers `SPL` (`0x3D`) and `SPH` (`0x3E`) to switch execution threads cleanly via `reti`.
- **Preemptive Priority-Based Scheduling (`src/task.c`, `src/scheduler.c`):**
  - Priority scheduling with round-robin tie-breaking among ready tasks of equal priority.
  - Slices tasks dynamically using 16-bit **Timer1** Output Compare Match B (`OCR1B`), waking the CPU at exact sub-millisecond deadlines rather than burning power on a coarse fixed tick.
- **Static Memory Architecture (Zero Fragmentation):**
  - Statically allocated 100-byte (96 usable) independent task stacks.
  - Safe slot tombstoning (`tasks[slot].id = TASK_ID_INVALID`) on task deletion, eliminating pointer invalidation and stack aliasing bugs.
- **Priority Inheritance Mutexes (`src/mutex.c`):**
  - Resource locks for microcontroller I/O ports (`portb_mutex`, `portc_mutex`, `portd_mutex`).
  - Dynamic priority inheritance elevates a lock owner's priority to that of the highest blocked contender, eliminating unbounded priority inversion.
- **Software Memory Protection Unit (MPU Emulation) (`src/stack.c`):**
  - Multi-byte guard fences (`0xA5`) at the base of every task stack.
  - Contiguous struct packaging (`guarded_stack_t`) in the `.data` section with `0xDEADBEEF` / `0xBEEFDEAD` boundary sentinels, physically guaranteeing memory adjacency in silicon.
  - Runtime canary verification on every preemption and yield.
- **Multi-Victim Self-Healing & Hardware Watchdog Recovery:**
  - **Quarantine:** Faulty tasks that breach their stack boundary are isolated into `TASK_FAULTED` and stripped of held mutexes.
  - **In-Flight Task Reboot:** Downward stack blasts that trample adjacent victim tasks trigger an automated stack re-framing and task reboot from entry address without crashing the MCU.
  - **Catastrophic Fail-Stop:** Breaches past kernel boundary sentinels trigger a 2-second optical panic strobe on `ERROR_LED` followed by a hardware **Watchdog Timer (WDT)** system reset.

---

## Memory Footprint (ATmega328P)

| Memory Section | Footprint | Available Hardware | Utilization |
|---|---|---|---|
| **Flash ROM (`.text` + `.data`)** | ~6.07 KB | 32 KB | **~19%** |
| **Static RAM (`.data` + `.bss`)** | ~1.11 KB | 2 KB | **~55%** |
| **Dynamic Heap** | 0 Bytes | 2 KB | **0% (Purely Static)** |

---

## Hardware Pinout & Breadboard Mapping

The system runs standalone on a breadboard without requiring an Arduino board or USB connection:

| MCU Pin | AVR Port | Direction | Function / Peripheral |
|---|---|---|---|
| **Pin 1** | `PC6 / RESET` | Input | Hardware reset with 10 kΩ pull-up |
| **Pins 4, 5** | `PD2, PD3` | Input | `INT0` / `INT1` external pushbuttons (Internal 20 kΩ pull-ups) |
| **Pin 12** | `PD6` | Output | Hardware Timer0 fast PWM Breathing LED |
| **Pin 13** | `PD7` | Output | Kernel Power-On Indicator LED |
| **Pin 14** | `PB0` | Output | Error LED |
| **Pins 15..18**| `PB1..PB4` | Output | 4-bit Binary Counter LEDs (`COUNTER_MASK`) |
| **Pin 19** | `PB5` | Output | Independent Periodic Heartbeat Blink LED |
| **Pins 9, 10** | `XTAL1, XTAL2`| Clock | 16 MHz Crystal Oscillator with two 22 pF ceramic caps |

---

## Directory Structure

```
micrOS-328/
├── include/
│   ├── functions.h        # Worker tasks and application prototypes
│   ├── interrupt.h        # External interrupt configurations (INT0/INT1)
│   ├── mutex.h            # Mutex definitions and priority inheritance APIs
│   ├── pwm.h              # Fast PWM LED drivers
│   ├── scheduler.h        # Scheduler initialization and ready-queue APIs
│   ├── stack.h            # Guarded stack structs, sentinels, and recovery prototypes
│   ├── task.h             # Task control blocks (TCBs), states, and stack parameters
│   └── time.h             # 16-bit Timer1 hardware ticks and prescaler math
├── src/
│   ├── context_switch.S   # Assembly register save/restore (r0-r31, SREG, SP)
│   ├── functions.c        # Application tasks (counter, blink, breathe, pattern, supervisor)
│   ├── interrupt.c        # EIMSK / EICRA falling-edge trigger setups
│   ├── isr.c              # Hardware ISR vectors (INT0, INT1, TIMER1_OVF, TIMER1_COMPB)
│   ├── main.c             # Hardware init, task registration, and kernel boot
│   ├── mutex.c            # Priority inheritance locking and lock-free ownership
│   ├── pwm.c              # Timer0 8-bit Fast PWM configuration
│   ├── scheduler.c        # Dynamic time slicing and ready list updates
│   ├── stack.c            # Canary verification, auto-reboot, and WDT panic trap
│   ├── task.c             # Static stack allocation, task yields, delays, tombstoning
│   └── time.c             # Timer1 prescaler setup and hardware tick queries
├── Makefile               # Cross-compilation toolchain configuration (avr-gcc)
└── README.md              # Project documentation and architectural overview
```

---

## Build and Flash Instructions

### Prerequisites
- `avr-gcc`, `avr-objcopy`, and `avr-size` (e.g. Microchip AVR 8-bit Toolchain / WinAVR).
- `avrdude` for flashing over an ISP programmer or serial bootloader.
- `make` or `mingw32-make`.

### Compilation
```bash
# Clean previous builds
make clean

# Compile ELF and generate HEX firmware
make all
```

Output files will be generated in `build/main.elf` and `build/main.hex`.

### Flashing to Standalone ATmega328P
Using an Arduino as ISP or USBasp programmer:
```bash
avrdude -c usbasp -p m328p -U flash:w:build/main.hex:i
```
*(Or adjust `-c` and `-P` according to your specific programmer).*

---

## Interactive Breadboard Demonstrations

1. **Concurrent Preemption:**
   - 4-bit binary counter (`PB1`–`PB4`), independent heartbeat blinker (`PB5`), and PWM breathing LED (`PD6`) run simultaneously at differing priorities with zero jitter.
2. **Asynchronous External Interrupt Event (`INT0`):**
   - Pressing the `INT0` button signals the supervisor task to preempt lower-priority tasks and run an optical Knight Rider pattern across the counter LEDs.
3. **Simulated Memory Violation & Self-Healing (`INT1`):**
   - Pressing `INT1` triggers an uncontrolled task that bursts past its stack limit.
   - The kernel detects the breached canary, isolates the offender into `TASK_FAULTED`, unlocks any held mutexes, cleanly reboots any damaged victim tasks in-flight, and pulses the Error LED (`PB0`) while keeping the rest of the OS running uninterrupted.
4. **Catastrophic Breach Fail-Stop:**
   - If an overflow breaches the global `.data` boundary sentinels, the OS freezes execution, pulses `ERROR_LED` in a fast 2-second optical strobe, and triggers a clean hardware reset via the **Watchdog Timer**.
