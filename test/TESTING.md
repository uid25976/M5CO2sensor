# CO2sensGen Unit Testing Guide

## Overview

This document describes the unit testing approach for the `CO2sensGen` class and provides instructions for running the tests.

## Test File Structure

### `test_CO2sensGen.cpp`

This file contains comprehensive unit tests for the `CO2sensGen` class using the Google Test framework with Google Mock for mocking hardware dependencies.

### Test Coverage

The tests cover the following functionality:

1. **Initialization Tests**
   - Constructor initialization
   - Sensor begin() success/failure scenarios
   - Measurement preparation

2. **Core Functionality Tests**
   - CO2 percentage calculation
   - Maximum CO2 tracking
   - Time since last measurement
   - Sensor name retrieval
   - Sensor range verification

3. **Measurement Tests**
   - Measurement failure handling
   - Data ready timeout scenarios
   - Complete measurement workflow

4. **Utility Function Tests**
   - CO2 max reset functionality

## Prerequisites

### For Host System Testing (Recommended)

1. **Google Test Framework**
   ```bash
   sudo apt-get install libgtest-dev libgmock-dev
   ```

2. **CMake** (version 3.10 or higher)
   ```bash
   sudo apt-get install cmake
   ```

3. **Build Tools**
   ```bash
   sudo apt-get install build-essential
   ```

### For Arduino/ESP32 Environment

For testing directly on Arduino/ESP32 platforms, consider:
- **Unity Test Framework** (lightweight C testing framework)
- **AUnit** (Arduino's built-in test framework)
- Appropriate mocking libraries for hardware dependencies

## Setting Up the Test Environment

### Option 1: Host System Testing with Mocks

1. **Create a CMakeLists.txt file**:

```cmake
cmake_minimum_required(VERSION 3.10)
project(CO2sensGenTests)

# Find Google Test
find_package(GTest REQUIRED)
find_package(GMock REQUIRED)

# Add your source files
add_executable(co2_tests
    test_CO2sensGen.cpp
    CO2sensGen.cpp
    CO2sensGen.h
)

# Link Google Test and Google Mock
target_link_libraries(co2_tests
    GTest::GTest
    GTest::Main
    GMock::GMock
)

# Include directories
target_include_directories(co2_tests PRIVATE
    ${GMOCK_INCLUDE_DIRS}
    ${GTEST_INCLUDE_DIRS}
)
```

2. **Build and run tests**:

```bash
mkdir build
cd build
cmake ..
make
./co2_tests
```

### Option 2: Arduino/ESP32 Testing

For embedded testing, you'll need to:

1. **Create test stubs** for hardware dependencies
2. **Use a lightweight testing framework** like Unity
3. **Mock I2C and sensor communications**
4. **Run tests on the target hardware**

## Test Implementation Notes

### Mocking Strategy

The tests use mock objects to replace hardware dependencies:

- **MockI2C**: Mocks I2C communication
- **MockSCD4X**: Mocks the SCD4X sensor class

### Test Limitations

Some tests are limited due to:

1. **Private member access**: Tests can't directly access private members like `firstValidData`
2. **Hardware dependencies**: Real sensor communication requires mocking
3. **Timing dependencies**: Tests involving `millis()` require time mocking

### Improving Testability

To make the class more testable:

1. **Add friend classes** for test access to private members
2. **Use dependency injection** for I2C and sensor objects
3. **Extract interfaces** for hardware dependencies
4. **Add test-specific constructors** that accept mocked dependencies

## Test Cases Description

### ConstructorInitialization
Verifies that the constructor properly initializes the object state.

### BeginFailure / BeginSuccess
Tests sensor initialization under both failure and success conditions.

### PrepareMeasurementsWakeupFailure / PrepareMeasurementsSuccess
Tests the measurement preparation sequence, including wakeup, reinitialization, and sensor configuration.

### CO2PercentageCalculation
Tests the conversion from ppm to percentage (400 ppm = 0.04%).

### CO2MaxTracking
Tests that the maximum CO2 value is properly tracked and updated.

### TimeSinceLastMeasurement
Tests the time tracking functionality for measurement intervals.

### SensorNameRetrieval
Tests that sensor type names are correctly returned.

### SensorRange
Verifies that the sensor range (4%) is correctly reported.

### MeasurementFailureHandling / DataReadyTimeout
Tests error handling in measurement scenarios.

### SuccessfulMeasurement
Comprehensive test of the complete measurement workflow.

### CO2MaxReset
Tests the reset functionality for maximum CO2 tracking.

## Running Tests in CI/CD

For continuous integration, add a test step to your workflow:

```yaml
# GitHub Actions example
- name: Run unit tests
  run: |
    mkdir build
    cd build
    cmake ..
    make
    ./co2_tests
```

## Future Test Improvements

1. **Add integration tests** with real hardware
2. **Expand edge case coverage** (boundary values, error conditions)
3. **Add performance tests** for timing-critical operations
4. **Implement property-based testing** for mathematical operations
5. **Add mocking for millis()** to test time-dependent behavior

## Contributing Tests

When adding new functionality to `CO2sensGen`:

1. **Write tests first** following the existing patterns
2. **Test both success and failure paths**
3. **Add edge case tests**
4. **Ensure 100% coverage** of new code
5. **Update this documentation** with new test descriptions

## Troubleshooting

### Common Issues

**Missing Google Test**: Install with `sudo apt-get install libgtest-dev libgmock-dev`

**Linking errors**: Ensure all source files are included in the build

**Mocking issues**: Verify that mock methods match the real interface signatures

**Permission denied**: Run tests with appropriate permissions for hardware access

## License

The test code is released under the same license as the main project (MIT).