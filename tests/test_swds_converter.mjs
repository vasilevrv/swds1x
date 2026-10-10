import assert from 'node:assert/strict';
import fs from 'node:fs';
import vm from 'node:vm';
const source = fs.readFileSync(new URL('../zigbee2mqtt/swds1x.mjs', import.meta.url), 'utf8');
const code = source.replace(/^import .*;\n/gm, '').replace(/export const /g, 'const ')
    .replace('export default', 'globalThis.definition =');
const numeric = (name, access) => ({name, access, withUnit(unit) {this.unit=unit;return this;}, withDescription(description) {this.description=description;return this;}});
const context = {batteryConverter: {cluster: 'genPowerCfg'}, exposes: {numeric, text: numeric, access: {STATE: 1}, presets: {contact: () => ({name: 'contact'}), battery: () => ({name: 'battery'}), battery_voltage: () => ({name: 'voltage'})}}};
vm.runInNewContext(code, context);
const d = context.definition;
assert.equal(d.fingerprint[0].modelID, 'swds1x');
assert.equal(d.fingerprint[0].manufacturerName, 'Salu Workshop');
assert.equal(d.exposes.length, 3);
const convert = d.fromZigbee[0].convert;
assert.equal(convert(null, {data: {zonestatus: 0}}).contact, true);
assert.equal(convert(null, {data: {zonestatus: 1}}).contact, false);
assert.equal(convert(null, {data: {zoneStatus: 0x10}}).contact, true);
assert.equal(convert(null, {data: {zoneStatus: 0x11}}).contact, false);
assert.equal(convert(null, {data: {}}), undefined);
assert.equal(d.fromZigbee.length, 2);
const calls = [];
const coordinator = {};
await d.configure({getEndpoint: (id) => {
    assert.equal(id, 1);
    return {bind: async (cluster, dst) => { assert.equal(dst, coordinator); calls.push(cluster); },
        configureReporting: async (cluster, records) => {
            assert.equal(cluster, 'genPowerCfg');assert.equal(records.length,2);
            assert.equal(records[0].attribute,'batteryPercentageRemaining');
            assert.equal(records[0].maximumReportInterval,0xFFFF);
            assert.equal(records[0].minimumReportInterval,0);
            assert.equal(records[1].minimumReportInterval,0);
            assert.equal(records[1].maximumReportInterval,0xFFFF);assert.equal(records[1].attribute,'batteryVoltage');calls.push('reporting');},
        read: async (cluster, attrs) => {
            if(cluster==='ssIasZone') assert.equal(attrs[0], 'zoneStatus');
            else {assert.equal(cluster,'genPowerCfg');assert.equal(attrs[1],'batteryPercentageRemaining');}
            calls.push('read'); }};
}}, coordinator);
assert.deepEqual(calls, ['ssIasZone', 'read', 'genPowerCfg', 'reporting', 'read']);
console.log('PASS: swds1x fingerprint, IAS open/closed decoding, battery exposes and configure bind/report/read');
