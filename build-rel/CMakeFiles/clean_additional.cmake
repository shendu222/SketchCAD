# Additional clean files
cmake_minimum_required(VERSION 3.16)

if("${CONFIG}" STREQUAL "" OR "${CONFIG}" STREQUAL "Release")
  file(REMOVE_RECURSE
  "AbiProbePlugin_autogen"
  "CMakeFiles\\AbiProbePlugin_autogen.dir\\AutogenUsed.txt"
  "CMakeFiles\\AbiProbePlugin_autogen.dir\\ParseCache.txt"
  "CMakeFiles\\SamplePlugin_autogen.dir\\AutogenUsed.txt"
  "CMakeFiles\\SamplePlugin_autogen.dir\\ParseCache.txt"
  "CMakeFiles\\SketchBenchmark_autogen.dir\\AutogenUsed.txt"
  "CMakeFiles\\SketchBenchmark_autogen.dir\\ParseCache.txt"
  "CMakeFiles\\SketchCADAutomation_autogen.dir\\AutogenUsed.txt"
  "CMakeFiles\\SketchCADAutomation_autogen.dir\\ParseCache.txt"
  "CMakeFiles\\SketchCAD_autogen.dir\\AutogenUsed.txt"
  "CMakeFiles\\SketchCAD_autogen.dir\\ParseCache.txt"
  "CMakeFiles\\SketchSelfTest_autogen.dir\\AutogenUsed.txt"
  "CMakeFiles\\SketchSelfTest_autogen.dir\\ParseCache.txt"
  "SamplePlugin_autogen"
  "SketchBenchmark_autogen"
  "SketchCADAutomation_autogen"
  "SketchCAD_autogen"
  "SketchSelfTest_autogen"
  )
endif()
