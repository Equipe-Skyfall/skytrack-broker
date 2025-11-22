const fs = require("fs");
const path = require("path");

jest.mock("mqtt", () => {
  const { EventEmitter: mockEventEmitter } = require("events");

  return {
    connect: jest.fn(() => {
      const c = new mockEventEmitter();
      c.subscribe = jest.fn((topic, cb) => cb && cb(null));
      c.end = jest.fn();
      c.connected = false;
      return c;
    })
  };
});

jest.mock("express", () => {
  return () => ({
    listen: jest.fn((port, cb) => cb && cb()),
    use: jest.fn()
  });
});

jest.mock("mongodb", () => {
  return {
    MongoClient: jest.fn().mockImplementation(() => ({
      connect: jest.fn().mockResolvedValue(true),
      db: () => ({
        collection: () => ({
          insertOne: jest.fn().mockResolvedValue(true)
        })
      })
    }))
  };
});

describe("dist/server.js content checks", () => {
  const filePath = path.resolve(__dirname, "..", "dist", "server.js");

  test("file exists and contains expected classes and methods", async () => {
    const content = await fs.promises.readFile(filePath, "utf8");

    expect(content).toMatch(/class\s+MessageQueue/);
    expect(content).toMatch(/class\s+BatchProcessor/);
    expect(content).toMatch(/class\s+ESP32MQTTServer/);

    expect(content).toMatch(/enqueue\(/);
    expect(content).toMatch(/dequeueBatch\(/);
    expect(content).toMatch(/markProcessed\(/);

    expect(content).toMatch(/server\.start\(\)/);
    expect(content).toMatch(/process\.on\('SIGINT'/);
  });
});
