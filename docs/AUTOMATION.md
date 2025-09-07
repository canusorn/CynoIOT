# CynoIoT Automation System Documentation

## Overview

The CynoIoT automation system allows you to create rule-based automation that responds to sensor data changes. Each automation rule can trigger GPIO control actions or emit events based on conditional logic.

## Automation Rule Format

Each automation rule consists of 9 components separated by colons (`:`):

```
actionType : variable : operator : threshold : target : mode : value [: timeLimit] [: timeoutValue]
```

Multiple rules are separated by commas (`,`).

### Component Details

1. **actionType** - Type of action to execute:
   - `gpio` or `g` - GPIO control action
   - `event` or `e` - Event emission action

2. **variable** - Name of the variable to monitor (must match variable names set with `setkeyname()`)

3. **operator** - Comparison operator:
   - `>` - Greater than
   - `<` - Less than
   - `>=` - Greater than or equal
   - `<=` - Less than or equal
   - `==` - Equal to

4. **threshold** - Threshold value to compare against the variable

5. **target** - Action target:
   - For GPIO: GPIO pin number
   - For events: Event name to emit

6. **mode** - GPIO control mode (ignored for events):
   - `digit` - Digital output (HIGH/LOW)
   - `pwm` - PWM output (0-255)
   - `dac` - DAC output (ESP32 only, 0-255)

7. **value** - Action value:
   - For digital GPIO: `1` (HIGH) or `0` (LOW)
   - For PWM/DAC: 0-255 duty cycle value
   - For events: Value to send with event

8. **timeLimit** (optional) - Duration in seconds to maintain the action before reverting

9. **timeoutValue** (optional) - Value to set when timeLimit expires (same mode as original action)

## Examples

### Basic GPIO Control
```
gpio:temperature:>:30:2:digit:1
```
- When temperature > 30°C, set GPIO pin 2 to HIGH (digital)

### PWM Control with Timeout
```
gpio:light:<:200:4:pwm:128:10:0
```
- When light < 200 lux, set GPIO pin 4 to PWM value 128 for 10 seconds, then set to 0

### Event Emission
```
event:humidity:<:40:low_humidity::alert
```
- When humidity < 40%, emit event "low_humidity" with value "alert"

### Multiple Rules
```
gpio:temperature:>:30:2:digit:1:5:0,event:humidity:<:40:moisture_alert::warning,gpio:light:>:800:4:pwm:255
```

## Usage in Code

### 1. Set up variables
```cpp
String keyname[] = {"temperature", "humidity", "light"};
cynoiot.setkeyname(keyname, 3);
```

### 2. Configure automation rules
Automation rules are typically received via MQTT topic `/{deviceId}/automation`, but can also be set programmatically for testing.

### 3. Update sensor data
```cpp
float sensorData[] = {temperature, humidity, lightLevel};
cynoiot.update(sensorData);  // This triggers automation checking
```

### 4. Handle events (optional)
```cpp
void onEvent(String event, String value) {
  Serial.println("Event: " + event + " = " + value);
}

void setup() {
  cynoiot.setEventCallback(onEvent);
}
```

## Implementation Details

### Rule Parsing
- Rules are parsed by splitting the automation string on commas for individual rules
- Each rule is split on colons to extract the 9 components
- All components are trimmed of leading/trailing whitespace
- Minimum 7 components required for valid rule (timeLimit and timeoutValue are optional)

### Condition Evaluation
- Rules are processed in the order they appear in the automation list
- Each rule is evaluated against the current sensor data
- Only rules matching the updated variable are processed
- Conditions are evaluated using floating-point comparison

### Action Execution
- GPIO actions are queued in the internal GPIO buffer for processing
- Event actions are queued in the internal event buffer
- Actions are executed in the main `handle()` loop

### Timeout Handling
- Time-limited actions are tracked in an internal timeout array
- Timeouts are checked every second in the main `handle()` loop
- When a timeout expires, the specified timeout action is executed
- Timeout tracking is automatically cleaned up after execution

### Buffer Management
- GPIO and event buffers can hold up to 10 pending actions each
- If buffers are full, new actions are ignored with debug messages
- Buffers are processed and cleared in each `handle()` cycle

## Debugging

Enable debugging by defining `CYNOIOT_DEBUG` to see automation execution logs:

```cpp
#define CYNOIOT_DEBUG
```

Debug messages include:
- Rule parsing and validation
- Condition evaluation results
- Action execution confirmations
- Timeout setup and execution
- Buffer status messages

## Limitations

- Maximum 10 concurrent timeout-tracked actions
- GPIO/event buffers limited to 10 pending actions each
- Timeout precision is 1 second (checked every second)
- Variable names must exactly match those set with `setkeyname()`
- Floating-point precision limitations for condition evaluation

## Error Handling

The system handles various error conditions gracefully:
- Invalid rule formats are skipped with debug messages
- Unknown operators are ignored
- Buffer overflow conditions are logged
- Missing timeout values disable timeout functionality for that rule