# Vigier

![CI](https://github.com/shff/ge/workflows/Rust/badge.svg)

Vigier is an minimalist, opinionated, game framework. It provides windowing, 3D graphics, controls, audio, asset loading, bundling and dev tools. The aim is to provide functionality required by most games but with a very small API. It trades flexibility by simplicity.

The abstraction layers use lower-level languages and are are simple to understand. The API is declarative and decouples the low-level aspects from the game code. Give it a scene graph and it will handle the low-level aspects of each renderer. Worry only about the high-level aspects.

It supports MacOS, iOS, Win32, X11, Android and Web. It comes with a tool for real-time development. It also handles the bundling and for each platform.

## LICENSE

```
MIT License

Copyright (c) 2021 Silvio Henrique Ferreira

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```
