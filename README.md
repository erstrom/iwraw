# iwraw

iwraw is essentially a stripped down version of iw where all netlink attribute
handling has been removed.
This means that the user must create the netlink attribute stream (nla stream) with
a separate tool (such as nljson-decoder) and pass that stream to iwraw when sending
messages to the kernel.
The same thing applies when receiving messages. The netlink attributes in any received
message will be written to stdout in raw format and must be interpreted by another
program (such as nljson-encoder)

iwraw has two modes of operation:

* Send/receive NL80211 commands
* Listen for NL80211 events

## How to build

iwraw uses cmake.

It is recommended to use a separate build directory in order not to mix
the build artifacts with the source code.

Basic build using default configuration:

```sh
mkdir build
cd build
cmake ..
make
```

### Dependencies

iwraw is linked against libgenl-3.0

### Cross compilation

The easiest way of cross compiling is to create a toolchain file with all necessary
cmake variables defined and then invoke cmake with -DCMAKE_TOOLCHAIN_FILE

Toolchain file:

```cmake
SET(CMAKE_SYSTEM_NAME Linux)

SET(CMAKE_C_COMPILER <path to c-compiler>)
SET(CMAKE_FIND_ROOT_PATH <path to cross-toolchain staging root dir>)
SET(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM ONLY)
SET(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
SET(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
```

Build command:

```sh
mkdir build
cd build
cmake -DCMAKE_TOOLCHAIN_FILE=<path to toolchain file> ..
make
```

## iwraw modes of operation

### Send command mode

iwraw will construct an nl80211 message with a user specified command id, append a
user supplied nla stream and send the message to the kernel.
In case there is a response message from the kernel, the netlink attributes in the
response message will be written to stdout.

The send command mode is activated when the user passes an nl80211 command to the
program (-c | --command)

### Listen for events mode

iwraw will listen for events if no nl80211 command is specified on the command line.
As soon as an event is received, iwraw will extract the netlink attributes from the
event and print the content to stdout

The listen for events mode is activated when no nl80211 command is passed to the
program (typically, the program is launched without any options or arguments at all)

## Interpreting the received data

The receive data can be piped to another program for analysis.
Below is an example where iwraw is used to listen for events. The received event
attributes are passed to nljson-encoder. nljson-encoder encodes the event attributes
into a JSON representation (with 4 spaces indentation) using the attribute policy
definition specified by nl80211-attr-policy.json

Please consult the nljson documentation for more info on how to use nljson-encoder
and how to write policy definitions.

```sh
iwraw | nljson-encoder -f4 -p nl80211-attr-policy.json
```

## Intended usage

The main benefit of using iwraw is when transmitting and receiving vendor commands
and events.

Vendor commands are used by some network drivers to write proprietary data, like
special commands to the driver.

Typical examples of vendor commands can be:

* enter/exit test mode of the hardware
* Configure special parameters such as transmit guard time intervals etc.

Vendor events can be raised by a wireless driver when the other nl80211 event families
are not suitable for the event. This could be anything from temperature warnings to
 DCC/CBR warnings.

Since vendor commands and events are very non-generic (each driver has its own definitions),
it could be useful to move the data interpretation (parsing of netlink attributes) from iw
to another program.

This is essentially what iwraw does.

## Usage together with nljson

Below is a list of examples of how to use iwraw together with nljson.
All examples involve vendor commands and events.

### Example 1: Transmit vendor command

Below is a JSON definition of a simple vendor command.
It contains three attributes; VENDOR_ID, VENDOR_SUBCMD and VENDOR_DATA.
All vendor commands are expected to look like this, otherwise nl80211
will not pass the command to the driver.

In order for the below example to work, the mac80211_hwsim kernel module
must be loaded.

VENDOR_ID identifies the driver. In the below example the id is 4980=0x1374
which is the vendor ID for QCA. This is the vendor id used by mac80211_hwsim.

VENDOR_DATA holds the actual command payload. In this particular case it is
a nested attribute (NLA_NESTED) since mac80211_hwsim expects this.

vendor-hwsim-subcmd-1.json:

```json
{
    "NL80211_ATTR_VENDOR_ID": {
        "data_type": "NLA_U32",
        "nla_type": 195,
	"value": 4980
    },
    "NL80211_ATTR_VENDOR_SUBCMD": {
        "data_type": "NLA_U32",
        "nla_type": 196,
        "value": 1
    },
    "NL80211_ATTR_VENDOR_DATA": {
        "data_type": "NLA_NESTED",
        "nla_type": 197,
        "value": {
            "QCA_WLAN_VENDOR_ATTR_TEST": {
                "data_type": "NLA_U32",
                "nla_type": 8,
                "value": 5
            }
        }
    }
}
```

The below command first reads the JSON vendor command and passes it to nljson-decoder
on stdin. Then, nljson-decoder decodes the JSON data into a raw nla stream and passes that
data to iwraw. iwraw creates an nl80211 vendor command, adds the device index attribute for
wlan0 and appends the raw nla stream read from stdin to the message. Finally, the message
is transmitted to the kernel.

Any received attributes will be redirected to /dev/null so no output will be printed.

```sh
cat vendor-hwsim-subcmd-1.json | nljson-decoder | iwraw -c vendor --interface wlan0 > /dev/null
```

### Example 2: Transmit vendor command and interpret the response

Example 2 is an extension of example 1 where the received response is interpreted.

Instead of redirecting the output to /dev/null the output is piped to nljson-encoder.
nljson-encoder will encode the received nla stream into a JSON representation.
In order for nljson to be able to interpret the received data it needs a policy
definition.

vendor-hwsim-policy.json:

```json
{
    "NL80211_ATTR_VENDOR_DATA": {
        "data_type": "NLA_NESTED",
        "nla_type": 197,
        "nested": {
            "SUBCMD_8": {
                "data_type": "NLA_U32",
                "nla_type": 8
            }
        }
    }
}
```

The policy definition tells nljson-encoder how to interpret NL80211_ATTR_VENDOR_DATA.
mac80211_hwsim will add a nested attribute into the vendor data attribute so the
policy defintion will contain a nested policy for NL80211_ATTR_VENDOR_DATA.

Below is the command line:

```sh
cat vendor-hwsim-subcmd-1.json | nljson-decoder | iwraw -c vendor --interface wlan0 | nljson-encoder -f4 -p vendor-hwsim-policy.json
```

This is the JSON output from nljson-encoder:

```json
{
    "UNKNOWN_ATTR_1": {
        "data_type": "NLA_UNSPEC",
        "nla_type": 1,
        "nla_len": 4,
        "value": [
            2,
            0,
            0,
            0
        ]
    },
    "NL80211_ATTR_VENDOR_DATA": {
        "data_type": "NLA_NESTED",
        "nla_type": 197,
        "nla_len": 8,
        "value": {
            "SUBCMD_8": {
                "data_type": "NLA_U32",
                "nla_type": 8,
                "nla_len": 4,
                "value": 7
            }
        }
    }
}
```

Since attribute 1 is not part of the policy definition it will end up as
UNKNOWN_ATTR_1 in the JSON output.

### Example 3: Listen for vendor events

When iwraw is called without any arguments it will listen for nl80211
events and print the received attributes to stdout.
nljson-encoder will encode the nla stream into JSON using the same
policy file that was used in example 2.

```sh
iwraw | nljson-encoder -f4 -p my-event-policy.json -p vendor-hwsim-policy.json
```

The output will be very verbose since there is a lot of attributes in an
nl80211 event message!

Below is the encoded output from a mac80211_hwsim event:

```json
{
    "UNKNOWN_ATTR_1": {
        "data_type": "NLA_UNSPEC",
        "nla_type": 1,
        "nla_len": 4,
        "value": [
            2,
            0,
            0,
            0
        ]
    },
    "UNKNOWN_ATTR_195": {
        "data_type": "NLA_UNSPEC",
        "nla_type": 195,
        "nla_len": 4,
        "value": [
            116,
            19,
            0,
            0
        ]
    },
    "UNKNOWN_ATTR_196": {
        "data_type": "NLA_UNSPEC",
        "nla_type": 196,
        "nla_len": 4,
        "value": [
            1,
            0,
            0,
            0
        ]
    },
    "UNKNOWN_ATTR_153": {
        "data_type": "NLA_UNSPEC",
        "nla_type": 153,
        "nla_len": 8,
        "value": [
            1,
            0,
            0,
            0,
            2,
            0,
            0,
            0
        ]
    },
    "UNKNOWN_ATTR_3": {
        "data_type": "NLA_UNSPEC",
        "nla_type": 3,
        "nla_len": 4,
        "value": [
            7,
            0,
            0,
            0
        ]
    },
    "NL80211_ATTR_VENDOR_DATA": {
        "data_type": "NLA_NESTED",
        "nla_type": 197,
        "nla_len": 8,
        "value": {
            "SUBCMD_8": {
                "data_type": "NLA_U16",
                "nla_type": 8,
                "nla_len": 4,
                "value": 6
            }
        }
    }
}
```

In order to reduce the amount of info we can use the -s (or --skip-unknown) flag with nljson-encoder.
This will make nljson-encoder ignore all unknown attributes (attributes not present in the policy file).

```sh
iwraw | nljson-encoder -f4 -p my-event-policy.json -p vendor-hwsim-policy.json -s
```

The output will now only contain attributes present in the policy file:

```json
{
    "NL80211_ATTR_VENDOR_DATA": {
        "data_type": "NLA_NESTED",
        "nla_type": 197,
        "nla_len": 8,
        "value": {
            "SUBCMD_8": {
                "data_type": "NLA_U32",
                "nla_type": 8,
                "nla_len": 4,
                "value": 6
            }
        }
    }
}
```

