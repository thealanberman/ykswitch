// ykswitch - block/unblock YubiKey OTP keystrokes by seizing its HID keyboard
// interface. Run as root: `ykswitch off` forks a holder process; `ykswitch on`
// kills it, which releases the device. `ykswitch on -t N` re-blocks after N
// seconds via a detached timer process.
//
// build: clang -O2 -o ykswitch ykswitch.c -framework IOKit -framework CoreFoundation

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/hid/IOHIDManager.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define PIDFILE "/var/run/ykswitch.pid"
#define TIMERFILE "/var/run/ykswitch.timer.pid"
#define YUBICO_VID 0x1050

static int seized;

static void set_int(CFMutableDictionaryRef d, CFStringRef k, int v) {
    CFNumberRef n = CFNumberCreate(NULL, kCFNumberIntType, &v);
    CFDictionarySetValue(d, k, n);
    CFRelease(n);
}

static void on_match(void *ctx, IOReturn r, void *sender, IOHIDDeviceRef dev) {
    if (IOHIDDeviceOpen(dev, kIOHIDOptionsTypeSeizeDevice) == kIOReturnSuccess)
        seized++;
}

static pid_t read_pid(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    int pid = 0;
    fscanf(f, "%d", &pid);
    fclose(f);
    if (pid > 0 && (kill(pid, 0) == 0 || errno == EPERM)) return pid;
    return 0;
}

static pid_t holder_pid(void) { return read_pid(PIDFILE); }

static void cancel_timer(void) {
    pid_t pid = read_pid(TIMERFILE);
    if (pid) kill(pid, SIGTERM);
    unlink(TIMERFILE);
}

static void detach(void) {
    setsid();
    freopen("/dev/null", "r", stdin);
    freopen("/dev/null", "w", stdout);
    freopen("/dev/null", "w", stderr);
}

static void hold(int report_fd) {
    IOHIDManagerRef mgr = IOHIDManagerCreate(NULL, kIOHIDOptionsTypeNone);
    CFMutableDictionaryRef m = CFDictionaryCreateMutable(NULL, 0,
        &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    set_int(m, CFSTR(kIOHIDVendorIDKey), YUBICO_VID);
    set_int(m, CFSTR(kIOHIDDeviceUsagePageKey), kHIDPage_GenericDesktop);
    set_int(m, CFSTR(kIOHIDDeviceUsageKey), kHIDUsage_GD_Keyboard);
    IOHIDManagerSetDeviceMatching(mgr, m);
    CFRelease(m);
    // Stays registered, so a key plugged in later is seized too.
    IOHIDManagerRegisterDeviceMatchingCallback(mgr, on_match, NULL);
    IOHIDManagerScheduleWithRunLoop(mgr, CFRunLoopGetMain(), kCFRunLoopDefaultMode);

    CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.3, false);
    write(report_fd, &seized, sizeof seized);
    close(report_fd);
    CFRunLoopRun();
}

static int cmd_off(void) {
    cancel_timer();
    if (holder_pid()) { puts("already off"); return 0; }
    int p[2];
    if (pipe(p) != 0) { perror("pipe"); return 1; }
    pid_t pid = fork();
    if (pid < 0) { perror("fork"); return 1; }
    if (pid == 0) {
        close(p[0]);
        detach();
        FILE *f = fopen(PIDFILE, "w");
        if (f) { fprintf(f, "%d\n", getpid()); fclose(f); }
        hold(p[1]);
        _exit(0);
    }
    close(p[1]);
    int n = -1;
    if (read(p[0], &n, sizeof n) != sizeof n) {
        fprintf(stderr, "holder failed to start\n");
        return 1;
    }
    if (n > 0) printf("off (%d device%s blocked)\n", n, n == 1 ? "" : "s");
    else puts("off (no YubiKey present; will block when plugged in)");
    return 0;
}

static int cmd_on(long secs) {
    cancel_timer();
    pid_t pid = holder_pid();
    if (pid) kill(pid, SIGTERM);
    unlink(PIDFILE);
    if (secs <= 0) { puts("on"); return 0; }
    pid = fork();
    if (pid < 0) { perror("fork"); return 1; }
    if (pid == 0) {
        detach();
        FILE *f = fopen(TIMERFILE, "w");
        if (f) { fprintf(f, "%d\n", getpid()); fclose(f); }
        sleep(secs);
        unlink(TIMERFILE);
        _exit(cmd_off());
    }
    printf("on for %lds\n", secs);
    return 0;
}

int main(int argc, char **argv) {
    const char *cmd = argc > 1 ? argv[1] : "status";
    if (!strcmp(cmd, "status")) {
        puts(holder_pid() ? "off" : "on");
        return 0;
    }
    if (geteuid() != 0) { fprintf(stderr, "ykswitch %s: must run as root\n", cmd); return 1; }
    if (!strcmp(cmd, "off")) return cmd_off();
    if (!strcmp(cmd, "on")) {
        if (argc == 2) return cmd_on(0);
        char *end;
        long secs = argc == 4 && !strcmp(argv[2], "-t") ? strtol(argv[3], &end, 10) : 0;
        if (secs > 0 && !*end) return cmd_on(secs);
    }
    if (!strcmp(cmd, "toggle")) return holder_pid() ? cmd_on(0) : cmd_off();
    fprintf(stderr, "usage: ykswitch [on [-t seconds]|off|toggle|status]\n");
    return 2;
}
