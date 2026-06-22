# Spatialization

## Spatial Playback Instances
A playback instance is non-spatial by default. To make it spatial, you must explicitly enable it for that instance.
```c++
engine->SetPlaybackSpatial(explosionPlayback, true); // Enable spatialization for this instance
```

## Listeners
The listener represents the ears of the player. A listener is typically attached to the main camera. The DALIA Engine
supports up to 4 concurrent listeners. Multiple listeners can be very useful when creating split-screen multiplayer
experiences. If multiple listeners are active, each spatial playback instance will evaluate its final volume based on
the closest listener. By default, listener `0` is active at the origin `(0, 0, 0)`. To activate or de-activate a 
listener, use `Engine::SetListenerActive()`.
```c++
engine->SetListenerActive(1, true); // Activate listener 1
```
*Note: De-activating all listeners will result in spatial playback instances becoming inaudible.*

### Listener Parameter Control
To correctly spatialize audio, the engine needs to know where the listener is in the 3D world. It also needs to know
its orientation. Therefore, the listener attributes have to be continuously updated to match the state of your scene.
This can be done either by using the combined `Engine::SetListener3DAttributes()` function, or by individually setting
position, orientation, and velocity with their dedicated setter functions. Note that setting the velocity is not
necessary (and can be left as its default value) if you do not intend to utilize the Doppler effect.
```c++
// Assuming you place the listener on your camera
dalia::Listener3DAttributes attributes = MakeListener3DAttributes(
    {camera.position.x, camera.position.y, camera.position.z},
    {camera.forward.x, camera.forward.y, camera.forward.z},
    {camera.up.x, camera.up.y, camera.up.z},
    {camera.velocity.x, camera.velocity.y, camera.velocity.z}
);

engine->SetListener3DAttributes(0, attributes); // Update the attributes of listener 0
```
*Note: This example uses a helper function to create the struct. It is completely fine to create the struct
yourself. You can safely leave the `distanceProbePosition` to use its default value if you do not intend to use that
feature.*

## Distance Attenuation
As a sound source moves further away from the listener, it gets quieter. This volume drop-off is controlled by the
attenuation curve and the minimum and maximum distance boundaries of the playback instance.

### Minimum and Maximum Distances
The **Minimum Distance** (or **Min Distance**) represents the radius around the playback position where the volume is at 
its maximum (this would be the same volume as if the sound was non-spatial).

The **Maximum Distance** (or **Max Distance**) represents the radius where the volume drops to zero.
```c++
// The explosion sound will be heard at full volume within 7 meters and drop to zero after 100 meters
engine->SetPlaybackMinMaxDistance(explosionPlayback, 7.0f, 100.0f); 
```

### Attenuation Curves
The attenuation curve of the playback instance represents how the volume drops off between the minimum and maximum
distance. By default, all playback instances use the `AttenuationCurve::InverseSquare` curve.
```c++
// Set explosion to use a linear attenuation curve
engine->SetPlaybackAttenuationCurve(explosionPlayback, dalia::AttenuationCurve::Linear);
```

### Distance Probes
By default, distance is measured from the position of a playback instance to the position of the listener 
(typically the camera). However, in some genres (e.g. third-person games), this is typically not the desired behavior. 
The engine therefore provides the option to separate the attenuation position of the listener from its panning
position. To do this, set the distance probe position of the listener. Then set the `DistanceMode` of any playback 
instances you want to use the probe to evaluate its distance attenuation to `DistanceMode::FromDistanceProbe`.
```c++
// Set the distance attenuation probe position to the player character position
Vec3 desiredPosition = { player.position.x, player.position.y, player.position.z};
engine->SetListenerDistanceProbePosition(0, desiredPosition);

// Make sure that the playback distance attenuation uses the probe position we just set
engine->SetPlaybackDistanceMode(explosionPlayback, dalia::DistanceMode::FromDistanceProbe);
```

## The Doppler Effect
The Doppler effect simulates the change in frequency when a sound source moves relative to the listener. To use the Doppler
effect, both the playback instance and the listener must be supplying their velocities to the engine every frame.

### Enabling Doppler
You must explicitly enable the Doppler effect on a playback instance. It is turned off by default.
```c++
engine->SetPlaybackUseDoppler(explosionPlayback, true); // Enable the Doppler effect for this playback instance
```

### Doppler Scaling
You can exaggerate or reduce the intensity of the Doppler pitch shift. This can be done both globally and per-playback.
```c++
engine->SetGlobalDopplerFactor(2.0f); // Exaggerate the Doppler pitch shift globally

engine->SetPlaybackDopplerFactor(explosionPlayback, 0.5f); // Reduce the Doppler pitch shift locally
```
*Note: The global and local scales are multiplicative and both affect the final output at all times.*

## Multi-Listener Routing
If you are building a game using multiple listeners, you can choose which listeners are allowed to hear a specific
playback instance by using a `ListenerMask`. This is a bitmask, meaning that it can be combined using the bitwise `OR`
(`|`) operator. You can create a `ListenerMask` using the helper function `MakeListenerMask()` or by using the existing
constants provided by the API.
```c++
dalia::ListenerMask mask = MASK_LISTENER_0 | MASK_LISTENER_1;
engine->SetPlaybackListenerMask(explosionPlayback, mask); // Playback will only be heard by listeners 0 and 1
```
