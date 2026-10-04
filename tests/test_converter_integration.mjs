// Optional transport-format check with the actual herdsman packages.
// node tests/test_converter_integration.mjs /tmp/relay-2ch-node-check
import assert from 'node:assert/strict';
import {createRequire} from 'node:module';
import {mkdtempSync, readFileSync, writeFileSync, rmSync} from 'node:fs';
import {join, resolve} from 'node:path';
import {pathToFileURL} from 'node:url';
const root = resolve(process.argv[2] ?? '.');
const require = createRequire(join(root, 'package.json'));
const {prepareDefinition} = require('zigbee-herdsman-converters');
const {Zcl} = require('zigbee-herdsman');
const temporary = mkdtempSync(join(root, 'relay-test-'));
try {
    const copy = join(temporary, 'relay_2ch.mjs');
    writeFileSync(copy, readFileSync(new URL('../zigbee2mqtt/relay_2ch.mjs', import.meta.url)));
    const {default: original} = await import(pathToFileURL(copy));
    original.externalConverterName = 'relay_2ch.mjs';
    const definition = prepareDefinition(original);
    assert.equal(definition.model, 'swrelay2x');
    assert.deepEqual(definition.endpoint(), {l1: 1, l2: 2});
    const exposes = Array.isArray(definition.exposes) ? definition.exposes : definition.exposes({isDummyDevice: true}, {});
    const properties = exposes.flatMap((expose) => expose.features ? expose.features.map((feature) => feature.property) : [expose.property]);
    for (const property of ['state_l1', 'state_l2', 'power_on_behavior_l1', 'power_on_behavior_l2', 'button_1_linked', 'button_2_linked', 'button_1_double_click', 'button_2_double_click', 'action']) assert(properties.includes(property), property);
    const customClusters = {};
    const device = {addCustomCluster: (name, cluster) => {customClusters[name] = cluster;}};
    // Reloaded converter: neither start nor configure has run for this device.
    assert.throws(() => Zcl.Utils.getCluster('relay2Config', undefined, customClusters), /UNSUPPORTED_CLUSTER/);
    device.getEndpoint = (id) => ({
        write: async (name, values) => {
            const cluster = Zcl.Utils.getCluster(name, undefined, customClusters);
            assert.equal(cluster.ID, 0xfc00); assert.equal(id, 1);
            assert.deepEqual(values, {buttonLinked: true});
        },
        read: async (name, attrs) => {
            const cluster = Zcl.Utils.getCluster(name, undefined, customClusters);
            assert.equal(cluster.ID, 0xfc00); assert.equal(id, 1);
            return {[attrs[0]]: true};
        },
    });
    const setter = original.toZigbee[0];
    assert.deepEqual(await setter.convertSet(null, 'button_1_linked', true, {device}), {state: {button_1_linked: true}});
    delete customClusters.relay2Config;
    await setter.convertGet(null, 'button_1_linked', {device});
    // First extension installs the custom cluster before the device's frames are decoded.
    await original.extend[0].configure[0](device);
    const converter = definition.fromZigbee.find((item) => item.type.includes('commandButtonEvent'));
    for (const endpoint of [1, 2]) for (const event of [0, 1, 2]) {
        const buffer = Buffer.from([0x19, endpoint * 10 + event, 0x00, event]);
        const header = Zcl.Header.fromBuffer(buffer);
        const frame = Zcl.Frame.fromBuffer(0xfc00, header, buffer, customClusters);
        assert.equal(frame.command.name, 'buttonEvent');
        assert.deepEqual(frame.payload, {event});
        assert.deepEqual(converter.convert(definition, {endpoint: {ID: endpoint}, data: frame.payload}), {action: `button_${endpoint}_${['single', 'double', 'hold'][event]}`});
    }
    const cluster = Zcl.Utils.getCluster('relay2Config', undefined, customClusters);
    assert.equal(Zcl.Utils.getClusterAttribute(cluster, 'buttonLinked').type, Zcl.DataType.BOOLEAN);
    const write = Zcl.Frame.create(0, 0, false, undefined, 1, 'write', 'relay2Config', [{attrId: 1, dataType: 0x10, attrData: 1}], customClusters).toBuffer();
    assert.deepEqual([...write], [0, 1, 2, 1, 0, 0x10, 1]);
    console.log(`PASS: actual zigbee-herdsman-converters ${require('zigbee-herdsman-converters/package.json').version}: external definition validation, exposes, endpoint mapping, custom cluster, six wire events, Boolean write encoding`);
} finally {
    rmSync(temporary, {recursive: true, force: true});
}
