# Daikin AGS AC Remote fro ESPHome

A external ESPHome climate component for Daikin AGS remote compatible Air Conditioners.

## Installation

**Short version:**

Just follow instructions at [https://esphome.io/components/external_components](https://esphome.io/components/external_components).

Or see [example_component.taml](example).

**Longer version**

1. Load external component

Load external component by adding following code to your device config file:

```yaml
external_components:
  - source:
      type: git
      url: https://github.com/atomicus/esphome-daikin-ags
    components: [daikin_ags]
```

Define climate device by setting it as in example:

```yaml
climate:
  - platform: daikin_ags
    name: "Living Room AC"
```
