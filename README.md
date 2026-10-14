# ykswitch

Stop a YubiKey from typing OTP codes when its touch sensor is brushed, and turn it back on, from the command line on macOS.

ykswitch takes exclusive hold of the YubiKey's HID keyboard interface (Yubico vendor ID `0x1050`), so keystrokes from the key never reach macOS. FIDO/WebAuthn, PIV, and other non-keyboard functions are unaffected, and nothing is written to the key.

## Build

Needs only the Xcode Command Line Tools.

```
clang -O2 -o ykswitch ykswitch.c -framework IOKit -framework CoreFoundation
```

## Usage

```
sudo ykswitch off      # block typing
sudo ykswitch on       # allow typing
sudo ykswitch on -t 30 # allow typing for 30 seconds, then block again
sudo ykswitch toggle
ykswitch status        # prints on|off; no root needed
```

`off` forks a background process that holds the key and records its PID in `/var/run/ykswitch.pid`. A key plugged in while it's running is blocked too. `on` kills that process, which releases the key. `on -t N` also forks a timer process (PID in `/var/run/ykswitch.timer.pid`) that runs `off` after N seconds; any later `on` or `off` cancels it.

## Why root

macOS only lets root take exclusive hold of a keyboard device. As a normal user the open fails with `kIOReturnNotPermitted` (`0xe00002c1`).

## Passwordless install

To call ykswitch from a macro without a password prompt:

```
./install.sh     # builds, installs /usr/local/bin/ykswitch (root:wheel 755), adds /etc/sudoers.d/ykswitch
./uninstall.sh   # re-enables the key and removes both
```

The sudoers rule lets your user run only `/usr/local/bin/ykswitch` as root without a password. That's safe only while the binary is root-owned and not writable by you; otherwise anything running as you could replace it and gain passwordless root. Don't point the rule at a copy in your home directory.

## Limitations

- Blocking ends at reboot. Run `off` again afterwards.
- Every Yubico keyboard interface is blocked; there's no per-product filter.
