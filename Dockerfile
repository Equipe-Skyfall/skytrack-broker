
# ----------------------------------------------------------------
# Build stage - Install dependencies and compile TypeScript
# ----------------------------------------------------------------
FROM node:18-alpine AS builder

WORKDIR /app


COPY package*.json ./


RUN npm ci


COPY . .

RUN npm run build



FROM node:18-alpine AS production


LABEL maintainer="ESP32 Sensor Project"
LABEL description="MQTT to MongoDB sensor data server"
LABEL version="1.0.0"

WORKDIR /app
RUN addgroup -g 1001 -S nodejs && \
    adduser -S nodejs -u 1001


COPY package*.json ./


RUN npm ci --omit=dev && \
    npm cache clean --force

COPY --from=builder /app/dist ./dist

RUN chown -R nodejs:nodejs /app

USER nodejs


EXPOSE 3000


HEALTHCHECK --interval=30s --timeout=3s --start-period=10s --retries=3 \
  CMD node -e "require('http').get('http://localhost:3000/health', (res) => { process.exit(res.statusCode === 200 ? 0 : 1) }).on('error', () => process.exit(1))"


CMD ["node", "dist/server.js"]