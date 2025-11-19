const fs = require('fs').promises;
const path = require('path');

describe('dist/server.js content checks', () => {
  const filePath = path.resolve(__dirname, '..', 'dist', 'server.js');

  test('file exists and contains expected classes and methods', async () => {
    const content = await fs.readFile(filePath, 'utf8');

    // Check for main classes
    expect(content).toMatch(/class\s+MessageQueue/);
    expect(content).toMatch(/class\s+BatchProcessor/);
    expect(content).toMatch(/class\s+ESP32MQTTServer/);

    // Check for important queue methods
    expect(content).toMatch(/enqueue\(/);
    expect(content).toMatch(/dequeueBatch\(/);
    expect(content).toMatch(/markProcessed\(/);

    // Check for start/stop server lifecycle
    expect(content).toMatch(/server\.start\(\)/);
    expect(content).toMatch(/process\.on\('SIGINT'/);
  });
});
