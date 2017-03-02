# Terra 
Unidirectional Monte Carlo Path Tracer  
co-authored w/ https://github.com/sparkon

### Features
- Header only (C99 ~2.6k lines of code) tested on **Mingw64-6.2.0** **Clang-3.9.0** **MSVC2015**
- Monte Carlo unidirectional integrator (global illumination, soft shadows, ambient occlusion, etc..)
- Accelerators:
    - BVH (Slower than K-D Tree for bigger scenes, but quicker at building)
    - K-D Tree
- Multiple BSDF presets:
    - Diffuse (importance sampled cosine weighted)
    - Microfacet layered CT glossy (importance sampled)
    - Glass (RR reflection/refraction)
    - Easy to add custom
- Texturing and HDR environment mapping
- Multiple importance sampling (direct light sampling)
- Russian Roulette for path termination
- Antialiasing via random sub-pixel jittering
- Multithread ready (see https://github.com/c4stan/TerraPreview)
- Incremental progressive rendering
- Multiple (local) tonemapping operators (Linear, Reinhard, Filmic & Uncharted2)

## Sample Renderings
#### Stanford dragon ~300k triangles (BSDF: Glass)
##### Specs
**Disclamer** 8 worker threads are used in addition to the main thread, this is not an optimal scenario.   
Machine: Intel Core i7-2600 @ 3.40GHz - Windows 10 Pro x64  
Worker threads: 8 (1 per virtual core)  

##### Stats
- `terra_trace()` ~ 4.2 ns per call
- Total Throughput ~ 1'670'000 ray/s

![](http://i.imgur.com/w4rndg8.jpg)

##### PBR Wood log (Textures: albedo/roughness/metalness) ~16k triangles (BSDF: Diffuse + Microfacet Specular)
![](http://i.imgur.com/jAwVDVg.jpg)
##### Stanford bunny ~70k triangles (BSDF: Diffuse + Microfacet Specular)
![](http://i.imgur.com/N6FEfsB.jpg)
