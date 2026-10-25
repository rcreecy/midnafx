// Observe an owned qualification fixture. Do not change arguments or returns.
const module = Process.getModuleByName('rea_fixture.exe');
const address = module.getExportByName('midnafx_rea_probe');
let calls = 0;
let mismatches = 0;
Interceptor.attach(address, {
    onEnter(args) {
        this.input = args[0].toUInt32();
    },
    onLeave(result) {
        const actual = result.toUInt32();
        const expected = (((this.input * 3) ^ 0x5a17) + 7) >>> 0;
        ++calls;
        if (actual !== expected) ++mismatches;
        if (calls <= 8) send({event: 'sample', input: this.input, actual, expected});
    }
});
send({event: 'ready', module: module.name, address: address.toString()});
setTimeout(() => send({event: 'summary', calls, mismatches}), 1200);
