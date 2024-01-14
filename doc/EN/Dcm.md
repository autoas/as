---
layout: post
title: AUTOSAR DCM
category: AUTOSAR
comments: true
---

# Configuration notes for DCM

Below 2 are examples:

* [bootloader/Dcm.json](../../app/bootloader/config/Dcm.json)
* [applocation/Dcm.json](../../app/app/config/Dcm.json)


## Session definitions

A list to describe all the supported sessions and its id value, simple and straight forward.

```json
  "sessions": [
    { "name": "Default", "id": "0x01" },
    { "name": "Program", "id": "0x02" },
    { "name": "Extended", "id": "0x03" },
    { "name": "Factory", "id": "0x50" }
  ]
```

## Security Level definition

```json
  "securities": [
    { "name": "Extended", "level": 1, "size": 4, "sessions": ["Extended"],
      "API": { "seed": "App_GetExtendedSessionSeed", "key": "App_CompareExtendedSessionKey"} },
    { "name": "Program", "level": 2, "size": 4, "sessions": ["Program"], 
      "API": { "seed": "App_GetProgramSessionSeed", "key": "App_CompareProgramSessionKey"} }
  ],
```

## Service definition

To know the details of how to define a serice, maybe better to understand how the Dcm generator works. This generator has a map "ServiceMap" which list all the supported service by this Dcm. 

[generator/Dcm.py](../../tools/generator/Dcm.py)

Common attributes for a service:

```json
    {
      "name": "service_name", "id": "service_id",
      "sessions":[...],
      "securities": [...],
      "access": [...]
      "API": ...
    },
```

| attibute | required | comments |
| --------- | ----------- |----|
| name | false | optional, can be anything,  just use it for easy reading purpose|
| service_id | true | must be unique and valid |
| sessions | false | a list of name of defined sessions that this service can be accessed, if empty, mean this service can be accessed in any session | 
| securities | false | a list of name of defined securities that this service can be accessed, if empty, mean this service can be accessed in any security level |
| access | false | supported choice "physical" or "functional", if empty, mean this service can be access by both physical and functional addressing mode |
| API | depends | API maybe optional, for different service it has different defintions |

For detailed how to define a service, check the above 2 examples.

### service Read Data By Identifier 0x22

Below is an example, you can see for this service, the attributes "sessions", "securities", "access" are missing. But it has another dedicated attribute "DIDs" which define a list of DID for this service.
```json
    {
      "name": "read_did", "id": "0x22",
      "DIDs":[
        { "name": "FingerPrint", "id":"0xF15B", "size": 10, "API": "App_ReadFingerPrint" },
        { "name": "TestDID1", "id":"0xAB01", "size": 10, "API": "App_ReadAB01" },
        { "name": "TestDID2", "id":"0xAB02", "size": 10, "API": "App_ReadAB02" }
      ]
    },
```

Common things for DID definition, please note it can also has attributes "sessions", "securities", "access" but which are all optional and which has the same meaning as the service definitions
```json
  { "name": "DID name", "id":"DID ID", "size": "DID size", "API": "DID callout API",
    "sessions":[...], "securities": [...], "access": [...] },
```

| attibute | required | comments |
| --------- | ----------- |----|
| name | false | optional, can be anything,  just use it for easy reading purpose|
| sessions | false | a list of name of defined sessions that this DID can be accessed, if empty, mean this DID can be accessed in any session | 
| securities | false | a list of name of defined securities that this DID can be accessed, if empty, mean this DID can be accessed in any security level |
| access | false | supported choice "physical" or "functional", if empty, mean this DID can be access by both physical and functional addressing mode |

### Memory Read/Write service

Below is a example:

```json
  "memories": [
    { "name": "Memory1", "low": 0, "high": "0x100000", "attr": "rw" },
    { "name": "Memory2", "low": "0x300000", "high": "0x400000", "attr": "r" }
  ],
  "memory.format": ["0x44"],
  "services": [
    { "name": "read_memory_by_address", "id": "0x23" },
    { "name": "write_memory_by_address", "id": "0x3D" }
  ]
```

Common things for memory definition, please note it can also has attributes "sessions", "securities", "access" but which are all optional and which has the same meaning as the service definition.

```json
  { "name": "MemoryName", "low": "low address", "high": "high address", "attr": "attributes, r means read, w mean write",
    "sessions":[...], "securities": [...], "access": [...] },
```


