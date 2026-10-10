# City Bloxx - C++17 / SDL3 port

A complete rewrite of the Java *CityBloxx* reconstruction (the J2ME game *City Bloxx* with its software
3D renderer) in modern C++17 with SDL3, using **the same resources** (`assets/` is the original
`assets/` folder with `manifest.txt`, PNG images, `.mid` audio, OBJ/MTL models and `lang/` string tables) and the **same game logic**.

> **Legal / educational note** (carried over from the original project): City Bloxx, Tower Bloxx and all related
> artwork, sounds and concepts belong to Digital Chocolate / Nokia. This port exists for preservation, study and
> interoperability, strictly non-commercial.

## Preview

![city bloxx decompiled ss 1](preview/1.png)
![city bloxx decompiled ss 2](preview/2.png)
![city bloxx decompiled ss 3](preview/3.png)
![city bloxx decompiled ss 4](preview/4.png)
![city bloxx decompiled ss 5](preview/5.png)
![city bloxx decompiled ss 6](preview/6.png)
![city bloxx decompiled ss 7](preview/7.png)
![city bloxx 3d models 1](preview/a.png)
![city bloxx 3d models 2](preview/b.png)
![city bloxx 3d models 3](preview/c.png)

## Build & run

Requires a C++17 compiler and SDL3 development files (or the included `./sdl3` folder on Windows).

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
./build/citybloxx            # assets are copied next to the binary
```
