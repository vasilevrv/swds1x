import assert from 'node:assert/strict';
import {mkdtempSync, cpSync, symlinkSync, rmSync} from 'node:fs';
import {tmpdir} from 'node:os';
import {join} from 'node:path';
import {pathToFileURL} from 'node:url';
const root = mkdtempSync(join(tmpdir(), 'swds-integration-'));
try {
    cpSync(new URL('../zigbee2mqtt/swds1x.mjs', import.meta.url), join(root, 'swds1x.mjs'));
    symlinkSync(join(process.argv[2], 'node_modules'), join(root, 'node_modules'));
    const {default: definition} = await import(pathToFileURL(join(root, 'swds1x.mjs')));
    assert.equal(definition.exposes[0].property, 'contact');
    assert.equal(definition.exposes[0].value_on, false);
    assert.equal(definition.exposes[0].value_off, true);
    assert.equal(definition.fromZigbee[0].convert(null, {data: {zonestatus: 1}}).contact, false);
    assert.equal(definition.fromZigbee[0].convert(null, {data: {zoneStatus: 0}}).contact, true);
    assert.equal(definition.exposes[1].property,'battery');
    assert.equal(definition.exposes[2].property,'voltage');
    const battery=definition.fromZigbee[1];
    const value=battery.convert(definition,{data:{batteryPercentageRemaining:115,batteryVoltage:29}},null,{},{});
    assert.equal(value.battery,57.5);assert.equal(value.voltage,2900);
    assert.equal(battery.convert(definition,{data:{batteryPercentageRemaining:255,batteryVoltage:255}},null,{},{}).battery,undefined);
    assert.equal(definition.exposes.length,3);
    assert.equal(definition.fromZigbee.length,2);
    // Decode the exact standard-only Report Attributes payload.
    const {Zcl} = await import(pathToFileURL(join(process.argv[2], 'node_modules/zigbee-herdsman/dist/index.js')));
    const {attributeKeyValue} = await import(pathToFileURL(join(process.argv[2], 'node_modules/zigbee-herdsman/dist/controller/helpers/zclFrameConverter.js')));
    const wire=Buffer.from('182a0a20002021210020c8','hex');
    const frame=Zcl.Frame.fromBuffer(1,Zcl.Header.fromBuffer(wire),wire,{});
    const data=attributeKeyValue(frame,0x1141,{});
    const decoded=battery.convert(definition,{data},null,{},{});
    assert.equal(decoded.voltage,3300);assert.equal(decoded.battery,100);
    console.log('PASS: real zigbee-herdsman-converters import and contact/battery/voltage exposes and actual half-percent decoding');
} finally {rmSync(root, {recursive: true, force: true});}
