// Observe only the selected host's exported GX boundary. No values are changed.
const host = Process.getModuleByName('dusklight.exe');
let warm = 0, restore = 0, other = 0, faults = 0;
Interceptor.attach(host.getExportByName('GXCallDisplayList'), {
    onEnter(args) {
        if (args[1].toUInt32() !== 32) return;
        try {
            const bytes = new Uint8Array(args[0].readByteArray(32));
            if (bytes[0] !== 0x10 || bytes[1] || bytes[2] || bytes[3] !== 0x10 || bytes[4] !== 0x0c) return;
            if (bytes[5] === 255 && bytes[6] === 240 && bytes[7] === 224 && bytes[8] === 255) ++warm;
            else if (bytes[5] === 255 && bytes[6] === 255 && bytes[7] === 255 && bytes[8] === 255) ++restore;
            else ++other;
        } catch (_) { ++faults; }
    }
});
send({event: 'ready', module: host.name, base: host.base.toString()});
setTimeout(() => send({event: 'summary', warm, restore, other, faults}), 1200);
