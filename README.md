# Neuroverse Engine / ParaBlit

## Goal

Neuroverse Engine is my attempt at creating a homebrew game engine.

The goal of Neuroverse is to create an advanced game engine with an aim with implementing modern cutting-edge rendering features whilst maintaining incredible performance. All with minimal manual optimization required of the end-user.

This is an ongoing passion project that is contributed to in my spare time, so it is far from complete.

Core development is done offline with a locally hosted perforce server. This git repository is a mirror intended to serve as a backup of Neuroverse's code and portfolio piece.

## Features & Technologies

Current features/technologies include:

  - Modular RenderGraph where individual render passes can be registered and request managed GPU resources. The lifetime and state of the GPU resources are automatically managed by the RenderGraph.
  - Physically based rendering with options to enable Raytraced Shadows & Reflections.
  - GPU-driven rendering, geometry culling and draw call batching with aims to minimize CPU overhead of geometry rendering and maximise GPU bandwidth utilization.
  - Custom rendering abstraction layer (ParaBlit) with Vulkan API backend.
  - Entity component system which through use of data reflection can modify and save relevant game state to XML exactly as represented at runtime, and restore that state on another run.
