# Bike Stamina System — Design Document

## Overview

Three interdependent physiological systems govern rider performance. Each operates on a different timescale and has a distinct physical expression. No system is fully independent — they compound in ways that reward tactical riding.

---

## Rider Stats (per rider)

| Stat | Description | Example values |
|------|-------------|---------------|
| `base_ftp` | Functional threshold power — max sustainable aerobic watts | 280W |
| `w_prime_max` | Anaerobic work capacity in joules | 20,000 J |
| `sprint_watts` | Hard ceiling on instantaneous power output, rider archetype stat | 900–1400W |
| `hr_rest` | Resting heart rate | 55 bpm |
| `hr_max` | Maximum heart rate | 185 bpm |

### Rider archetypes

| Archetype | sprint_watts | base_ftp | w_prime_max |
|-----------|-------------|----------|-------------|
| Sprinter  | 1400W | 270W | 18 kJ |
| Climber   | 900W  | 380W | 14 kJ |
| Rouleur   | 1100W | 320W | 22 kJ |

---

## System 1: Glycogen (Aerobic Fatigue)

### What it is
Long-term irreversible fuel reserve. Depletes over the course of a ride. As it depletes, `effective_ftp` drops. This is the resource that defines how a rider performs across a full race — not W'.

### Timescale
60–120 minutes to meaningful depletion at race intensity.

### Depletion
```
depletion_rate = base_drain * (power / effective_ftp)^1.5
```
Harder efforts cost disproportionately more glycogen. Sprinting (redlining) is the most glycogen-hungry effort.

### Recovery (forgiving model)
Partial passive recovery only at very low intensity (zone 1). Does not meaningfully recover mid-ride at any useful rate. No food management — the system is a slow background drain, not a resource to top up.

```
if power < ftp * 0.4:
    recovery_rate = slow_base_recovery
```

### Effect on FTP
```
effective_ftp = base_ftp * glycogen_factor   // glycogen_factor: 1.0 (fresh) → ~0.55 (cooked)
```

### Cascade into W'
Lower `effective_ftp` means more of the rider's output is "above FTP" — W' drains faster for the same wattage. A glycogen-depleted rider's sprints get shorter even with full W'.

### Player communication
- **Hidden** — no explicit meter or percentage shown
- **HR drift is the proxy**: same power = rising HR over time signals glycogen depletion
- **Qualitative "legs" descriptor**: `Fresh → Good → Fading → Struggling → Cooked`
- **Animation**: slow cumulative visual degradation (see Animation section)

---

## System 2: W' — Anaerobic Capacity

### What it is
The anaerobic work reservoir. Represents the total joules of work a rider can do above `effective_ftp`. Fully recoverable mid-ride given sufficient easy riding. This is the short-term borrowing mechanism against the aerobic engine.

### Timescale
Depletes in seconds to minutes above FTP. Recovers over ~10–15 minutes in zone 2.

### Depletion
```
if power > effective_ftp:
    w_prime_drain = (power - effective_ftp) * dt
```
Only watts above `effective_ftp` cost W'. Riding at or below FTP costs nothing.

### Recovery
```
if power < effective_ftp * 0.75:   // roughly zone 2 and below
    w_prime_recovery = recovery_rate * (1 - power / (effective_ftp * 0.75))
```
Zone 1 recovers fastest. Zone 2 recovers meaningfully. Above that, recovery stalls.

### Effect on max power
W' depletion reduces the rider's effective ceiling — it's not a binary cut-off. As W' drains, the burst power available above FTP scales down:

```
w_prime_fraction = w_prime_current / w_prime_max
power_ceiling = effective_ftp + (sprint_watts - effective_ftp) * w_prime_fraction
```

So a fully depleted rider is hard-capped at `effective_ftp`. A half-depleted rider can still sprint but not as hard or as long.

### Blow-up state
When W' hits zero, power doesn't just cap at FTP — there is a brief collapse below it before recovering to FTP. More realistic and more dramatic than a clean cap.

### Player communication
- **HR as primary signal**: HR staying elevated after backing off = W' not yet recovered. HR returning to zone 2 range = ready to go again.
- **Rough 3-state indicator** (secondary, optional): vague legs/effort indicator. Not labeled as W'. Something like: `●●●  ●●○  ●○○  ○○○`
- The player should learn to read HR as their W' gauge over time — explicit meter is a training wheel

### Tactical consequence
A sprint early in a race costs W' (recoverable) but also costs glycogen (not recoverable). Sprinting repeatedly is fine for W' but silently degrades FTP via glycogen drain. This makes early attacks tactically expensive even if W' recovers.

---

## System 3: Heart Rate

### What it is
A lagging indicator of physiological stress. Not a resource — it doesn't deplete or gate anything directly. Its value is the *lag* and *drift* — it tells the player what their body is paying for effort, not what they're currently outputting.

### Model
```
hr_target = hr_rest + (hr_max - hr_rest) * power_fraction

// Asymmetric smoothing — slower to fall than rise
tau = (hr_current < hr_target) ? tau_rise : tau_fall
// tau_rise ~45s, tau_fall ~90s

hr_current → hr_target via damp_dt_independent(hr_target, hr_current, tau, dt)
```

### Cardiac drift
As glycogen depletes, HR at the same power slowly rises. This is the visible signal of aerobic fatigue.

```
hr_drift accumulates when power > easy_threshold
hr_drift decays slowly when power < easy_threshold
effective_hr = hr_current + hr_drift
```

### What HR communicates
| HR state | What it means |
|----------|--------------|
| HR high after backing off | W' not yet recovered — don't attack yet |
| HR rising at constant power | Aerobic fatigue accumulating — glycogen draining |
| HR at zone 2, stable | Safe to continue, W' recovering |
| HR pinned at max | Deep in W', glycogen burning fast |

### Player communication
- Displayed as BPM, colored by HR zone (blue → green → yellow → orange → red)
- Zone coloring matches the power zone palette
- Primary intuitive gauge for both W' recovery and aerobic fatigue state
- Optionally: BPM number pulses at actual heart rate

---

## Compound Interactions

### The fatigue cascade
```
Long hard effort
→ Glycogen depletes
→ effective_ftp drops
→ Same watts = more above FTP
→ W' drains faster
→ Shorter sprints
→ HR drifts higher at same power
→ Player sees HR signal even without understanding the math
```

### The race arc
| Race phase | Glycogen | W' | HR | Effective sprint |
|-----------|----------|-----|-----|-----------------|
| Start (fresh) | 100% | Full | Low | Full duration, full power |
| Mid race | 70% | Cycling normally | Moderate drift | Shorter, slightly weaker |
| Late race | 45% | Cycling but costs more | Noticeably high | Brief, significantly weaker |
| Cooked | 55% floor | Drains almost instantly | Pinned | Barely above FTP |

### Why early sprints are wasteful
W' recovers, so early attacks are fine for W'. But glycogen lost in a sprint never comes back. A rider who burned two full sprints early arrives at the final climb with degraded FTP, meaning their "full W'" sprint at the end is weaker than a conservative rider's — even if W' values are identical.

---

## Player Communication Summary

| System | Meter shown | Signal | Player learns |
|--------|------------|--------|---------------|
| Glycogen | Hidden | HR drift, legs descriptor, animation | "My HR is higher than it should be — I'm fading" |
| W' | Rough 3-state indicator | HR recovery time | "Wait for HR to drop before attacking again" |
| HR | BPM + zone color | Everything | The primary race instrument |

The design intent is that HR becomes the instrument the player races by. Glycogen and W' are the underlying simulation — HR is how the player feels them.

---

## Animation — Physical Expression

Each system has a distinct animation character. They layer and compound visually.

### HR (fast, reactive)
- Breathing rate and heaviness increases
- Visible breath vapor in cold conditions gets faster/more desperate
- Slight head sway at very high HR
- Reacts quickly, matches HR lag — ramps up fast, lingers after effort

### W' (explosive, effortful)
- Shoulder rocking — pulling on bars during hard effort
- Standing out of saddle on sprints
- Cadence irregularity, pedal stroke loses smoothness
- Grimacing, clenched jaw
- Appears suddenly during efforts, fades relatively quickly as W' recovers

### Glycogen (slow, cumulative, irreversible-feeling)
- Sweat accumulation building over the ride
- Progressive postural slouch — shoulders dropping, back rounding
- Head drooping lower over time
- Face pallor, sunken/drawn look
- Pedal stroke becoming visibly less powerful
- **Does not snap back when effort eases** — communicates irreversibility

### The late-race visual
All three overlapping: rider drenched, slouched, rocking desperately on a climb, breathing ragged. The whole race is written on the character. This makes the animation layer function as a UI — glycogen state readable without any meter.
