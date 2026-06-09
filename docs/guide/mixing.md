# Mixing & Routing

## The Mixing Hierarchy
The mixing hierarchy exists to allow for dynamic runtime mixing. The hierarchy consists of buses. A bus is an audio node
that takes multiple input audio streams, mixes them together, and outputs a single combined stream. Buses are managed
entirely via string identifiers.

### The Master Bus
The `Engine` creates a permanently allocated `Master` bus on initialization. This bus serves as the root node for the
mixing hierarchy and all audio in the engine ultimately flows through it before reaching the speakers. The `Master` bus
cannot be destroyed or routed.

### Bus Management
To create a new bus, use `Engine::CreateBus()`. Every new bus must be given a unique identifier string. Optionally, you
can also provide the identifier string of the bus you wish the new bus to be routed to 
(it can be re-routed at any point). If no parent identifier is passed, the new bus will be routed to `Master`.
```c++
engine->CreateBus("SFX"); // New bus with identifier "SFX" routed to "Master"

engine->CreateBus("UI", "SFX"); // New bus with identifier "UI" routed to "SFX"
```
Buses are reference counted. If you call `Engine::CreateBus()` using an identifier that already exists, the engine will 
increment the internal reference count of that bus.

A bus can be re-routed at any point using `Engine::RouteBus()`:
```c++
engine->RouteBus("UI", "Master"); // Routing bus with identifier "UI" into "Master"
```
However, the mixing hierarchy has built-in cycle
protection. The engine will always guarantee that the routing remains a Directed Acyclic Graph (DAG). Attempts to create
a routing cycle will therefore always fail and return `Result::InvalidRouting`.

To destroy a bus, simply call `Engine::DestroyBus()`. As buses are reference counted, this function must be called the
same number of times as `Engine::CreateBus()` for the same identifier in order to be destroyed.
```c++
engine->DestroyBus("UI");
```
*Note: If a bus is destroyed, any child buses or active playbacks that are routed to the bus are orphaned. An orphaned 
bus can be rerouted to a new parent bus, but an orphaned voice will immediately be stopped by the engine with the exit 
condition `PlaybackExitCondition::ExplicitStop`.*

### Bus Parameter Control
The volume of a bus can be changed at any point. Doing this will identically scale the volume of every playback and
child bus routed into it.
```c++
engine->SetBusVolumeDb("SFX", -3.0f); // Lowers the volume of the "SFX" bus by 3dB
```

## Routing Playback Instances
By default, all new playback instances are routed directly to the `Master` bus. To route it to the desired bus in your
mixing hierarchy, use `Engine::RoutePlayback()`. This can be done at any point before or during playback.
```c++
engine->RoutePlayback(explosionPlayback, "SFX"); // Routes explosionPlayback to the "SFX" bus
```

## Effects
A bus does not just combine audio. It can also process it. Every bus has an Effects Rack with 4 effect slots
(indices `0` through `3`). Before the output of a bus is mixed into its parent, it is processed sequentially through the
effect slots in the order `0`, `1`, `2`, `3`.

### Creating an Effect
Effects must first be created and configured before being attached to a bus. Let's create a Lowpass filter.
```c++
// Filter that cuts off frequencies above 800Hz
dalia::BiquadParams lowpassParams;
lowpassParams.type = dalia::BiquadParams::Type::LowPass;
lowpassParams.frequency = 800.0f;
lowpassParams.resonance = 1.0f;

dalia::EffectHandle lowpassFilter;
engine->CreateEffect(lowpassFilter, lowpassParams);
```

### Attaching and Hot-Swapping
Once you have a valid `EffectHandle`, you can attach it to a specific slot on a bus.
```c++
engine->AttachEffect(lowpassFilter, "SFX", 0);
```
Effects are exclusively attached. A single effect instance can only be attached to one bus at a time. If you attach an
effect to a slot that is already occupied, the old effect is forcefully detached and replaced with the new one.

*Note: Hot-swapping an effect on a bus that is actively playing audio bypasses internal fade-outs and may cause "pops".
It is advised to only attach and detach effects when a bus is not actively playing audio.*

### Modifying and Detaching
The parameters of an effect can be altered at any time, even when it is actively processing audio.
```c++
lowpassParams.frequency = 400.0f;
engine->SetEffectParams(lowpassFilter, lowpassParams); // Lower the frequency cutoff to 400Hz
```
*Note: Changing the `BiquadParams::Type` of an active filter alters its DSP topology instantly. Unlike frequency and 
resonance, discrete topology changes are not smoothed and will cause an audible pop if audio is currently passing 
through. It is highly recommended to only change filter types during silence or when the filter is not attached.*

When the effect is no longer needed, it can be detached from the Effect Rack. Detaching an effect does not destroy it.
It simply unplugs it from the bus and can be re-attached to the same, or any other bus later.
```c++
engine->DetachEffect(lowpassFilter); // Detach the effect if it is currently attached
```
To destroy the effect and free its memory, simply call `Engine::DestroyEffect()`.
```c++
engine->DestroyEffect(lowpassFilter);
```
*Note: It is completely safe to destroy an effect that is attached to a bus. This will have the same effect as first
detaching the effect and then destroying it.*
