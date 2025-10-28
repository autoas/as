---
layout: post
title: JsonEditor for SSAS
category: AUTOSAR
comments: true
---


# JsonEditor for SSAS

The **JsonEditor for SSAS** is a flexible, PyQt5-based configuration tool designed to streamline the setup of SSAScomponents. While inspired by the open-source [JSON Editor](https://json-editor.github.io/json-editor/) (which dynamically generates GUIs from JSON schemas), this tool is tailored for automotive embedded systems, with specialized features to simplify SSAS BSW (Basic Software) configuration.

---

## Key Features & Purpose

### 1. **Schema-Driven GUI Generation**  
Like its open-source counterpart, the JsonEditor for SSAS automatically generates a user-friendly GUI based on a provided **JSON schema**. This schema defines the structure, constraints, and allowed values for SSAS configurations (e.g., BSW module parameters, communication settings, or diagnostic services).  

### 2. **Automatic Configuration Export**  
Once the user configures parameters via the GUI, the tool automatically generates **C/H source files**¡ªthe standard format for SSAS BSW configurations. This eliminates manual coding errors and accelerates development.  

### 3. **SSAS-Specific Enhancements**  
While not fully compliant with the full [JSON Schema Specification](https://json-schema.org/), the tool includes SSAS-specific extensions to better support automotive use cases. For example:  
- **`enumref`**: References predefined enumerations to enforce valid values for SSAS-specific parameters.  
- Custom validation rules for automotive timing constraints.  

---

## Quickstart: Using the JsonEditor for SSAS

### Step 1: Prepare a JSON Schema  
To start, you¡¯ll need a JSON schema that defines your SSAS configuration structure. You can either:  
- Use the [default SSAS schema](../../tools/json.editor/schema.json) (recommended for most use cases), or  
- Create a custom schema based on your project¡¯s requirements.  

For example, copy the schema from the [original JSON Editor demo](https://json-editor.github.io/json-editor/) (scroll to the bottom, click "Update Schema," then copy the JSON) and save it as `sc2.json`.  

![JSON Schema Example](../images/json-editor-schema-ex1.jpg)

### Step 2: Launch the JsonEditor for SSAS  
Navigate to the tool¡¯s directory and run the editor with your schema:  

```sh
# Launch with a custom schema (e.g., sc2.json)
cd tools/json.editor
python main.py -s sc2.json
```

A GUI window will open, rendering input fields, dropdowns, and other controls based on your schema. Configure parameters interactively (e.g., setting BSW module timeouts or PDU IDs).  

![JsonEditor Demo](../images/json-editor-example1.gif)

### Step 3: Generate SSAS Configuration Files  
After configuring parameters, save the settings. The tool automatically generates:  
- **C header/source files** (e.g., `Config.c`, `Config.h`) with the validated configuration values.  
- Documentation (optional) summarizing the configuration choices.  

---

## Running with the Default SSAS Schema  

For SSAS-specific projects, use the built-in `schema.json` to leverage pre-defined SSAS enumerations and constraints:  

```sh
# Launch with the default SSAS schema
cd tools/json.editor
python main.py
```

This loads the SSAS schema, enabling features like `enumref` for selecting predefined values.  

![SSAS JsonEditor Demo](../images/json-editor-ssas.gif)

---

## Notes & Limitations  

- **Schema Compatibility**: The tool supports core JSON Schema features (objects, arrays, enums) but may not implement all draft versions (e.g., `$ref` resolution is limited). Refer to [SSAS/schema](../../tools/json.editor/schema.json) for project-specific guidelines.  
- **SSAS Integration**: Generated C/H files are designed to integrate with the SSAS build system.  
- **Ease of Use**: Ideal for new developers¡ªno manual file parsing or complex scripting required to generate valid SSAS configurations.  

---