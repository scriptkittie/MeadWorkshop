cmake_minimum_required(VERSION 3.21)

project(MeadWorkshop VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Build only the math engine + its tests (no Qt needed). Used for checking the numbers anywhere.
option(MEAD_TESTS_ONLY "Build only the engine tests (no Qt required)" OFF)

# ---------------- Engine (plain C++17) ----------------
add_library(mead_engine STATIC
  src/engine/mead_engine.cpp src/engine/mead_engine.hpp
  src/engine/mead_advisor.cpp src/engine/mead_advisor.hpp
  src/engine/mead_text.cpp src/engine/mead_text.hpp
  src/engine/mead_words.cpp src/engine/mead_words.hpp
  src/engine/mead_spices.cpp src/engine/mead_spices.hpp
  src/engine/mead_json.hpp
)
target_include_directories(mead_engine PUBLIC src/engine)

add_executable(engine_tests tests/engine_tests.cpp)
target_link_libraries(engine_tests PRIVATE mead_engine)
add_executable(advisor_tests tests/advisor_tests.cpp)
target_link_libraries(advisor_tests PRIVATE mead_engine)
target_compile_definitions(advisor_tests PRIVATE MEAD_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")

enable_testing()
add_test(NAME engine_tests COMMAND engine_tests)
add_test(NAME advisor_tests COMMAND advisor_tests)

# Plans thousands of different meads and checks each against the books' rules.
add_executable(consistency_tests tests/consistency_tests.cpp)
target_link_libraries(consistency_tests PRIVATE mead_engine)
target_compile_definitions(consistency_tests PRIVATE MEAD_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
add_test(NAME consistency_tests COMMAND consistency_tests)

if(MEAD_TESTS_ONLY)
  return()
endif()

# ---------------- Qt application ----------------
find_package(Qt6 6.5 REQUIRED COMPONENTS Quick QuickControls2)
qt_standard_project_setup(REQUIRES 6.5)

qt_add_executable(MeadWorkshop
  src/main.cpp
  src/app/meadbridge.cpp src/app/meadbridge.hpp
  src/app/designer.cpp src/app/designer.hpp
  src/app/theme.hpp
)

# Qt 6.8+ asks how screens in a subfolder (qml/) should be found; NEW = give that folder its own qmldir.
if(QT_KNOWN_POLICY_QTP0004)
  qt_policy(SET QTP0004 NEW)
endif()

qt_add_qml_module(MeadWorkshop
  URI MeadWorkshop
  VERSION 1.0
  QML_FILES
    qml/Main.qml
    qml/Card.qml
    qml/Meter.qml
    qml/NumField.qml
    qml/Choice.qml
    qml/TextChoice.qml
    qml/PlanPage.qml
    qml/DesignerPage.qml
    qml/DirectionsPage.qml
    qml/QuickCheckPage.qml
    qml/Prose.qml
    qml/WordsPage.qml
    qml/SaveFirst.qml
)

# Reference data (from the books) — available in the app as :/data/<file>.json
qt_add_resources(MeadWorkshop "meaddata"
  PREFIX "/"
  FILES
    data/fruits.json
    data/yeasts.json
    data/schramm.json
    data/reference.json
    data/recipes.json
    packaging/icons/meadworkshop.png
)

# The app icon: in the .exe on Windows (Explorer, taskbar, installer)
if(WIN32)
  set(MEAD_ICON "${CMAKE_CURRENT_SOURCE_DIR}/packaging/icons/meadworkshop.ico")
  configure_file(packaging/windows/app.rc.in "${CMAKE_CURRENT_BINARY_DIR}/app.rc" @ONLY)
  target_sources(MeadWorkshop PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/app.rc")
endif()

target_link_libraries(MeadWorkshop PRIVATE Qt6::Quick Qt6::QuickControls2 mead_engine)
target_compile_definitions(MeadWorkshop PRIVATE MEAD_VERSION="${PROJECT_VERSION}")   # the one place the version is set

set_target_properties(MeadWorkshop PROPERTIES
  WIN32_EXECUTABLE TRUE        # no console window on Windows
  MACOSX_BUNDLE TRUE
)

include(GNUInstallDirs)
install(TARGETS MeadWorkshop
  BUNDLE DESTINATION .
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
if(UNIX AND NOT APPLE)   # Linux desktop entry and icon (used by the AppImage too)
  install(FILES packaging/linux/meadworkshop.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
  install(FILES packaging/icons/meadworkshop.png DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/256x256/apps)
endif()
