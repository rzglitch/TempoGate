# Third-Party Notices

This file lists third-party software used to build TempoGate.
TempoGate's own source code is under the MIT License (see `LICENSE`).
The dependencies below are governed by their own licenses.

---

## 1. JUCE 9 Framework

- **Project:** JUCE 9
- **Copyright:** Raw Material Software Limited
- **Website:** https://juce.com
- **License:** JUCE 9 End User Licence Agreement (commercial/proprietary EULA)
- **License URL:** https://juce.com/legal/juce-9-licence/

TempoGate is built against the JUCE 9 framework (expected at `~/JUCE`,
overridable with `-DJUCE_DIR=...`; see `CMakeLists.txt`). JUCE is not
vendored in this repository - it is consumed as an external build
dependency via `add_subdirectory()`.

JUCE 9 is licensed under the JUCE 9 End User Licence Agreement between
the licensee and Raw Material Software Limited. Available licence types
include Starter, Indie, Pro, and Educational, each with its own annual
revenue/funding limits, fees, and minimum commitments. Use of JUCE,
including combining it with other source code to produce a product such
as this plugin, requires a licence matching your circumstances. Refer to
the full agreement at the URL above for the binding terms, including
licensing scope, restrictions, fees, support, warranty disclaimer, and
termination.

---

## 2. VST3 SDK (bundled with JUCE)

- **Project:** VST 3 SDK by Steinberg Media Technologies GmbH
- **Copyright:** (c) 2025, Steinberg Media Technologies GmbH
- **Used via:** JUCE-bundled copy at
  `modules/juce_audio_processors_headless/format_types/VST3_SDK/`
- **License:** MIT License
- **License URL:**
  https://github.com/juce-framework/JUCE/blob/9.0.3/modules/juce_audio_processors_headless/format_types/VST3_SDK/LICENSE.txt

TempoGate enables the VST3 plugin target through JUCE, which in turn uses
the VST3 SDK headers bundled with JUCE. That SDK copy is MIT-licensed.
The full license text is reproduced below:

```text
-----------------------------------------------------------------------------
MIT License
Copyright (c) 2025, Steinberg Media Technologies GmbH

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
//---------------------------------------------------------------------------------
```
