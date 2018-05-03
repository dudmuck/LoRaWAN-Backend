

# LoRaWAN Backend

Standard system libraries used:
* [libcurl](https://curl.haxx.se/libcurl/) http client (for http post)
* [json-c](https://github.com/json-c/json-c/wiki) JSON generating and parsing
* [microhttpd](https://www.gnu.org/software/libmicrohttpd/) http server
* [gcrypt](https://gnupg.org/software/libgcrypt/index.html) Key Envelope `GCRY_CIPHER_MODE_AESWRAP`

HTTP headers contain `Accept: application/json` for JSON over HTTP, otherwise its considered to be web-browser.

## First-time database setup
Import `structure.sql` using phpmysql or using mysql command line.

## Build
Install dependencies:

raspbian/debian: `sudo apt-get install libjson-c-dev libgcrypt20-dev libmicrohttpd-dev libcurl-dev`

or RPM based: `sudo dnf install json-c-devel libgcrypt-devel libmicrohttpd-devel libcurl-devel`
* `mkdir build`
* `cd build`
* `cmake ..`
* `make`

Three executables are built, which must each be run separately, from the build directory:
* `./network_server`
* `./join_server`
* `./app_server`

## DNS setup
LoRaWAN-backend performs hostname lookups using NAPTR and SRV records.

Hosts can be added into json configure files to override DNS lookups or to skip requirement of requiring DNS lookups.  If you do not wish to setup DNS server, add `hosts` array to conf.json files.  If a join EUI or netID is found in conf.json, then DNS lookup is not performed.  Otherwise, if you want install DNS server, read the following:

For testing on your local machine, i.e. when using a NetID or JoinEUI which doesn't exist on internet DNS server, install BIND on your local machine.

Example named.conf and example zone files are provided here.  Modify your BIND server configuration files to add the example.com master zone as shown in named.conf, and point it to the example.com.zone file.

If using rasbpian for example, the DNS server package is called bind9, and files go into /etc/bind. 

All DNS lookups are using domain names provided in `conf.json` files: `joinDomain` and `netidDomain`.  The JoinEUI or NetID is prepended to these domain names at lookup.  When you are using your own DNS server, `conf.json` should have example.com domain, otherwise use lora alliance domain if you're using real NetID/JoinEUI.

When you are using fake example.com domain for lookups on your own DNS server, first line of /etc/resolv.conf must point to your DNS server.  When using DHCP client, resolve.conf is written to when leasing IP address, but this must be overridden to point to your own DNS instead.  The procedure for forcing fixed name server is resolv.conf varies for each operating system.

`./resove` test program will print DNS lookup result.

## Server Configuration
Each of the servers are configured locally using `conf.json` in the server's source directory.  `conf.json` is used for configuration changes which would require restarting the server when changes are made.  Other changes, such as configuring roaming NetIDs do not require restart of server.

### sessions
OTA devices sessions are created by join server when answering join request, for a lifetime stored on join server.
ABP devices have permanent session on network server, and do not involve join server.

## Provisioning end Devices
Each of the servers' httpd will serve basic html when web-browser points to the listening http port. This interface permits provisioning of end devices.  An OTA end device must be entered into both NS and JS.  Adding end device to NS establishes the forward-to-NetID (this NS = blank, other NS, or ask HomeNSReq), and profiles if the end device is home on NS.   

For lorawan-1.0 devices: FNwkSIntKey, SNwkSintKey, SNwkSintKey all contain the same value in database, but are collectively known as NwkSKey over JSON.
For lorawan-1.1 devices: FNwkSIntKey, SNwkSintKey, SNwkSintKey each have unique value.

ABP end devices don't involve the join server, because ABP has no concept of DevEUI/JoinEUI/root keys.

For adding ABP end device, only NwkAddr portion of DevAddr is entered into NS.  NwkAddr is the portion of DevAddr which is unique to each end device.  The resulting DevAddr is derived from both NwkAddr and NetID of NS.

Add ABP end-device to application server: enter DevAddr along with permanent AppSKey.

### OTA end-device provisioning steps:
1. Enter DevEUI into NS, using `createHome` if this NS is home NS.
2. Enter DevEUI and root key(s) into join server.  The JoinEUI of this this join server must match that of end-device.  Only NwkKey is provided for LoRaWAN-1.0.
3. Enter DevEUI only into AS.  When first uplink is sent from NS, the session key and DevAddr is automatically provided.
### ABP end-device provisioning steps:
1.  Enter NwkAddr portion of DevAddr into NS.  NwkAddr is the part of DevAddr which is unique to each end-device.   Use `createHome` button if this end-device is home on this NS.
2.  Enter network session key(s) into `SKeys`, if this end-device is home on this NS.  For LoRaWAN-1.0, all 3 network session keys are of same value.  For 1.1, all 3 are unique.
3.  Enter DevAddr and AppSKey into application server

To send/receive application payload, the end device must exist on application server. For OTA device, the DevEUI must be listed in application server prior to receiving first uplink. For ABP device, the DevAddr and AppSKey must exist in application server.


## Join Server
Point your browser at the `httpd_port` in `join_server/conf.json`  (default port 3000)

Join Server only applies to OTA end devices. 
LoRaWAN-1.0 devices only have NwkKey root key.
LoRaWaN-1.1 devices must have both root keys: NwkKey and AppKey.

## Application Server
Point your browser at the `httpd_port` in `app_server/conf.json`  (default port 4000)

Although the specification doesnt cover interface to AS, the same http-json messaging (as in the standard) is also used with AS for simplicity.

**Support of LoRaWAN 1.0:** Since AS only uses AFCntDown to encrypt downlink payload, AFCntDown will be synchronized to NFCntDown by NS rejecting FCntDown value from AS which mismatches NFCntDown. NS will send its NFCntDown to AS, then AS will use this frame counter for downlink encryption sent in a repeated attempt at downlink.

In LoRaWAN 1.1, none of this is needed, since NS passes through AFCntDown to end device.

## Network Server
Point your browser at the `httpd_port` in `network_server/conf.json`  (default port 2000)

Upon reception of (re)join, DevEUI must be in network server's list of DevEUIs or uplink will be dropped.  However, if an (un)confirmed uplink is received with a DevAddr for a network in the roaming list, a roam start request will be issued to the NetID that was  derived from DevAddr.

### network server gateway interface
The gateway interface is not covered in specification.   Gateway connection is TCP socket.  It only functions with [this packet forwarder](https://github.com/dudmuck/packet_forwarder/tree/master/forwarder).

### network server sessions
OTA end devices have a session expiration provided by join server at join-accept.  When lifetime expires for a 1.1 device, a force rejoin request will be sent to end device, causing a rejoin request from end device to create a new session. For 1.1 the old session is deleted when ReKey indication is received by NS.

When lifetime expires for 1.0 OTA device, the end device will be dropped off the network and can only send another join request.  For 1.0 ,the old session is removed when an (un)conf uplink is received passing MIC check.  The list of end devices may show a single end device more than once when a new session has been created, without old sessions being deleted by uplink.

In contrast, an ABP end-devices have permanent session, never expires.

### network server roaming
Via the simple browser interface to NS, to provision end device to operate on a visited network, home NetID of end device will be shown in "forward to NetID" column, or value of "HomeNSReq" if the join server is to be asked for home NetID of end device.  Use create button to add device without home profile.  For roaming to be allowed to the home NetID, this NetID must be listed on "networks" page, which determines roaming policy to that NetID.  The choice of passive vs handover roaming is established on "networks" page.
To provision end device on the other home NS, the createHome button must be used to add profiles for end device.
Roaming start: roam can be initiated by (re)join request from end device, or in the case of ABP by an (un)confirmed uplink.


## assumptions, or required variations from LoRaWAN-Backend specification
* **for passive-roaming fNS:** PRStartAns needs to contain PHYPayload, for downlink which might need to be sent to end device, such as join accept. (Table 13, HRStartAns already has PHYPayload listed as present)
* **For passive roaming start on OTA join request:** PRStartAns needs to contain DevAddr when its PHYPayload contains a join accept downlink, otherwise stateful fNS will have no way of identifying future uplinks.  Perhaps stateful fNS doesnt need DevEUI in PRStartAns, since (un)conf uplinks have DevAddr.
* ServiceProfile DRMin/DRMax refers to uplink.
* **AppSKey sent to AS:** since DevAddr is also used for payload encryption, DevAddr should be sent along with AppSKey.  DevAddr likely changes upon every (re)join.
* `RXDelay`, `RXDelay1` units are seconds?
* `DLFreq1` and `DLFreq2` will be in MHz units because `ULFreq` is in MHz
* DLMetaData (to sNS) will contain DevAddr for ABP end device, or DevEUI for OTA. (section 16.2)
* When (re)join is received while roaming is currently in effect, it will generate HRStartReq/PRStartReq instead of being forwarded as XmitDataReq, generating new session and restarting roaming.  Type0 rejoin is only taken if roaming has expired.  Type2 rejoin is only taken if force rejoin request was sent.
* PRStartAns/HRStartAns with Deferred result needs to provide DevAddr, so future uplinks can be ignored.  All roaming start answers with lifetime > 0 needs to provide DevAddr.

* GWInfo type in DL Packet Metadata table 5 declared as different type in table 22. list of ULToken parameters vs Array of GWInfoElement objects.
* **Class B support:** DLMetaData will contain PingPeriod integer when ClassMode == 'B'
* Class B ping slot period comes from end device via `PingSlotInfoReq`, not from `DeviceProfile.PingSlotPeriod`.

## issues to resolve (questions to answer)
* If AS returns fail code from uplink (unknown DevEUI, etc), if this uplink came from roaming partner, should such fail code be sent to partner?
* Session expiration of AppSKey: AS doesnt know when session expires, and must rely on NS to send new key.
* **for passive-roaming fNS:** PRStartAns needs to contain PHYPayload, for downlink which might need to be sent to end device, such as join accept. (HRStartAns already has PHYPayload listed as present)via thew AppSKey.  AS can generate downlink between new session creation and first uplink sent by end device.

## TODO:
* sNS: TxParamSetupReq, regional DownlinkDwellTime for RX1DROffset
* NS root web page: add select to choose rx1 or rx2 downlink. Also both could be enabled at same time.
* Should handover roaming start fail when roaming lifetime on home NS is zero?
* When session changes, any pending downlink from AS should be discarded with failure answer to AS.
* Careful attention to 1.1 downlinks with application payload, end-device might have different NFCntDown than sNS is using to encrypt FOpts.  Occurs when unconfirmed downlink is sent with NFCntDown, but not received by end-device.
* JS: 1v1 display and reset devNonce counters
* NS ABP: display and reset up/down frame counters
* implement DevStatusReqFreq
* generate ULMetaData: according to ServiceProfile: ULFreq?, DataRate?, Margin, Battery
* reject handover roaming when ED is already served by another NS with better signal. (perhaps passive also)
* Is curl multi-easy interface non-blocking enough for long DNS lookups?  https://github.com/c-ares/c-ares
* fNS GPS leap second https://www.andrews.edu/~tzs/timeconv/timealgorithm.html
* Support other standard or legacy gateway connections
* FCntUp sent only in PRStartAns, not HRStartAns.. why?  because HRStart is always at joinreq/acc, but PRStart is (un)conf


