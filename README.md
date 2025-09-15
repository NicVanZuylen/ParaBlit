
https://github.com/user-attachments/assets/f05959c1-7512-4192-b868-5a5f5f5e77a9
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

## Gallery

Raytraced Shadows & Reflections demonstrated with a small rotating scene including a Stanford Bunny and Blade Runner Spinner PBR Model on top of a material test plane.

https://github.com/user-attachments/assets/7f5ba4de-f6a0-4897-beec-67c3bac170b0

Just over 256 dynamic objects with LOD and real time transformations all drawn with mesh shaders and GPU-driven rendering with a single draw call.

https://github.com/user-attachments/assets/303abad9-77c0-4428-a026-b120c0b9adde

The dynamic objects as they are seen in RenderDoc's Mesh Viewer. Geometry is culled on a per-meshlet basis by frustrum and back face.

<img width="1782" height="978" alt="Screenshot 2025-09-15 190428" src="https://github.com/user-attachments/assets/d9706572-98b6-4562-abef-3e9a4593bb0b" />
