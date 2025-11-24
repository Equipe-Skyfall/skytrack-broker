import mqtt from 'mqtt';
import { MongoClient, Db, Collection } from 'mongodb';
import dotenv from 'dotenv';
import express from 'express';
import type { Request, Response } from 'express';


dotenv.config();

// Message Queue Manager
class MessageQueue {
  private collection!: Collection<any>;
  private queueStats = {
    received: 0,
    processed: 0,
    failed: 0,
    currentDepth: 0
  };

  async initialize(db: Db): Promise<void> {
    this.collection = db.collection('climate_queue');

    // Create index for efficient timestamp queries
    await this.collection.createIndex({ timestamp: 1 }); 

    // Get initial queue depth
    this.queueStats.currentDepth = await this.collection.countDocuments();
  }

  async enqueue(data: any): Promise<void> {
    await this.collection.insertOne({
      ...data,
      timestamp: new Date(),
      processed: false
    });
    this.queueStats.received++;
    this.queueStats.currentDepth++;
  }

  async dequeueBatch(batchSize: number): Promise<any[]> {
    const batch = await this.collection
      .find({ processed: false })
      .sort({ timestamp: 1 })
      .limit(batchSize)
      .toArray();

    return batch;
  }

  async markProcessed(ids: any[]): Promise<void> {
    const result = await this.collection.deleteMany({
      _id: { $in: ids }
    });

    this.queueStats.processed += result.deletedCount;
    this.queueStats.currentDepth -= result.deletedCount;
  }

  async markFailed(ids: any[]): Promise<void> {
    this.queueStats.failed += ids.length;
  }

  getStats() {
    return {
      ...this.queueStats,
      pendingRate: this.queueStats.received - this.queueStats.processed
    };
  }
}


class BatchProcessor {
  private queue: MessageQueue;
  private targetCollection: Collection<any>;
  private batchSize: number;
  private intervalMs: number;
  private processingInterval: NodeJS.Timeout | null = null;
  private isProcessing = false;
  private processingStats = {
    batchesProcessed: 0,
    recordsProcessed: 0,
    lastProcessedAt: new Date(),
    averageBatchTime: 0
  };

  constructor(
    queue: MessageQueue,
    targetCollection: Collection<any>,
    batchSize: number = 50,
    intervalMs: number = 1000
  ) {
    this.queue = queue;
    this.targetCollection = targetCollection;
    this.batchSize = batchSize;
    this.intervalMs = intervalMs;
  }

  start(): void {
    if (this.processingInterval) {
      return; 
    }

    this.processingInterval = setInterval(async () => {
      await this.processBatch();
    }, this.intervalMs);
  }

  stop(): void {
    if (this.processingInterval) {
      clearInterval(this.processingInterval);
      this.processingInterval = null;
    }
  }

  private async processBatch(): Promise<void> {
    if (this.isProcessing) {
      return; 
    }

    this.isProcessing = true;
    const startTime = Date.now();

    try {
      const batch = await this.queue.dequeueBatch(this.batchSize);

      if (batch.length === 0) {
        this.isProcessing = false;
        return;
      }

     
      const documents = batch.map(item => {
        const { _id, timestamp, processed, ...data } = item;
        return data;
      });

     
      await this.targetCollection.insertMany(documents, { ordered: false });

      
      const processedIds = batch.map(item => item._id);
      await this.queue.markProcessed(processedIds);

      
      this.processingStats.batchesProcessed++;
      this.processingStats.recordsProcessed += batch.length;
      this.processingStats.lastProcessedAt = new Date();

      const batchTime = Date.now() - startTime;
      this.processingStats.averageBatchTime =
        (this.processingStats.averageBatchTime * (this.processingStats.batchesProcessed - 1) + batchTime) /
        this.processingStats.batchesProcessed;

    } catch (error) {
      console.error('❌ Batch processing error:', error);
      
    } finally {
      this.isProcessing = false;
    }
  }

  getStats() {
    return {
      ...this.processingStats,
      isProcessing: this.isProcessing,
      batchSize: this.batchSize,
      intervalMs: this.intervalMs
    };
  }
}

class ESP32MQTTServer {
  private mqttClient: mqtt.MqttClient;
  private mongoClient: MongoClient;
  private db!: Db;
  private collection!: Collection<any>;
  private messageQueue!: MessageQueue;
  private batchProcessor!: BatchProcessor;
  private messageCount = 0;
  private app: express.Application;

  constructor() {
    const mongoUrl = process.env.MONGO_CONNECTION_STRING || 'mongodb://localhost:27017';

    this.mqttClient = mqtt.connect(process.env.MQTT_HOST!, {
  username: process.env.MQTT_USERNAME,
  password: process.env.MQTT_PASSWORD,
   rejectUnauthorized: false
});
    this.mongoClient = new MongoClient(mongoUrl);
    this.app = express();
    this.setupExpress();

      this.setupMQTT();
  }

  async start(): Promise<void> {
    try {
      await this.mongoClient.connect();
      console.log('✅ Connected to MongoDB');

      this.db = this.mongoClient.db(process.env.MONGO_DATABASE || 'dadosClima');
      this.collection = this.db.collection('clima');

    
      await this.collection.createIndex({ uuid: 1, unixtime: -1 });
      await this.collection.createIndex({ unixtime: -1 });

      
      this.messageQueue = new MessageQueue();
      await this.messageQueue.initialize(this.db);
      console.log('✅ Message queue initialized');


      const batchSize = parseInt(process.env.BATCH_SIZE || '100', 10);
      const batchInterval = parseInt(process.env.BATCH_INTERVAL_MS || '500', 10);

      this.batchProcessor = new BatchProcessor(
        this.messageQueue,
        this.collection,
        batchSize,
        batchInterval
      );

      this.batchProcessor.start();
      console.log(`✅ Batch processor started (batch size: ${batchSize}, interval: ${batchInterval}ms)`);

      this.startHttpServer();
    } catch (error) {
      console.error('❌ Failed to start server:', error);
      process.exit(1);
    }
  }

  private setupExpress(): void {
    this.app.use(express.json());

    this.app.get('/health', (req: Request, res: Response) => {
      res.status(200).json({
        status: 'healthy',
        timestamp: new Date().toISOString(),
        mqtt: this.mqttClient.connected ? 'connected' : 'disconnected',
        uptime: process.uptime()
      });
    });

    this.app.get('/stats', (req: Request, res: Response) => {
      res.json({
        messagesReceived: this.messageCount,
        mqttConnected: this.mqttClient.connected,
        uptime: process.uptime(),
        timestamp: new Date().toISOString()
      });
    });

    this.app.get('/queue-stats', (req: Request, res: Response) => {
      if (!this.messageQueue || !this.batchProcessor) {
        return res.status(503).json({
          error: 'Queue system not initialized'
        });
      }

      const queueStats = this.messageQueue.getStats();
      const processorStats = this.batchProcessor.getStats();

      res.json({
        queue: queueStats,
        processor: processorStats,
        health: {
          queueDepth: queueStats.currentDepth,
          processingLag: queueStats.received - queueStats.processed,
          isHealthy: queueStats.currentDepth < 10000
        },
        timestamp: new Date().toISOString()
      });
    });
  }

  private startHttpServer(): void {
    const port = process.env.PORT || 3000;
    this.app.listen(port, () => {
      // console.log(`🌐 HTTP server running on port ${port}`);
      // console.log(`🩺 Health check: http://localhost:${port}/health`);
    });
  }

  //subscreve no mqtt broker
  private setupMQTT(): void {
    this.mqttClient.on('connect', () => {
      console.log('🔗 Connected to MQTT broker');
      this.mqttClient.subscribe('weather/+/data', (err) => {
        if (err) {
          console.error('❌ Failed to subscribe:', err);
        } else {
          console.log('📡 Subscribed to weather/+/data');
        }
      });
    });

    this.mqttClient.on('message', async (topic: string, message: Buffer) => {
      // console.log(`📨 Received MQTT message on topic: ${topic}`);
      // console.log(`   Payload: ${message.toString()}`);
      try {
        await this.processMessage(topic, message);
        this.messageCount++;

        if (this.messageCount % 100 === 0) {
          console.log(`📊 Processed ${this.messageCount} messages`);
        }
      } catch (error) {
        console.error('❌ Error processing message:', error);
      }
    });

    this.mqttClient.on('error', (error) => {
      console.error('❌ MQTT error:', error);
    });
  }

  private async processMessage(topic: string, message: Buffer): Promise<void> {
    const topicParts = topic.split('/');
    if (topicParts.length !== 3 || topicParts[0] !== 'weather' || topicParts[2] !== 'data') {
      // console.log(`⚠️  Message ignored - topic doesn't match pattern: ${topic}`);
      return;
    }

    try {
      const messageData = JSON.parse(message.toString());
      // console.log(`✅ Message validated and queuing...`);

      await this.messageQueue.enqueue(messageData);
      // console.log(`✅ Message queued successfully`);

      if (this.messageCount % 100 === 0) {
        const queueStats = this.messageQueue.getStats();
        console.log(`📊 Received ${this.messageCount} messages | Queue depth: ${queueStats.currentDepth}`);
      }

    } catch (error) {
      console.error('❌ Failed to process message:', error);
    }
  }

  async stop(): Promise<void> {
    console.log('🔄 Shutting down gracefully...');

   
    this.mqttClient.end();


    if (this.batchProcessor) {
      this.batchProcessor.stop();
      console.log('✅ Batch processor stopped');
    }

  
    await new Promise(resolve => setTimeout(resolve, 2000));

    
    await this.mongoClient.close();
    console.log('🛑 Server stopped');
  }
}

const server = new ESP32MQTTServer();

server.start().catch(console.error);

process.on('SIGINT', async () => {
  // console.log('\n🔄 Shutting down server...');
  await server.stop();
  process.exit(0);
});

process.on('SIGTERM', async () => {
  // console.log('🔄 Shutting down server...');
  await server.stop();
  process.exit(0);
});