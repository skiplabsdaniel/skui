import express from "express";

const app = express().use(debug_no_cors).use(logger);
app.use(express.json({ strict: false }));

const port = 8666;

// Streaming control API.

app.get("/v1/service", (req, res) => {
  try {
    const info = {
      name: `127.0.0.1:${port}`,
      inputs: ["sessions"],
      remotesResources: [],
      resources: ["r1", "r2"],
      version: '"Dummy "(2025 06)""',
      platform: "wasm",
    };
    res.status(200).json(info);
  } catch (e) {
    console.log(e);
    res.status(500).json(e instanceof Error ? e.message : e);
  }
});

app.get("/v1/shared", (_req, res) => {
  try {
    const graph = {
      entities: [
        {
          inputs: [
            "/sk/mappers/remote/postgres/posts/b64_eyJrZXkiOiB7ImNvbCI6ICJpZCIsICJ0eXBlIjogIlNFUklBTCJ9fQ../",
          ],
          name: "/sk/mappers/map/ext_7/",
          reads: [
            "/sk/mappers/map/ext_6/",
            "/sk/mappers/remote/postgres/users/b64_eyJrZXkiOiB7ImNvbCI6ICJpZCIsICJ0eXBlIjogIlNFUklBTCJ9fQ../",
          ],
          type: {
            constructors: [
              {
                name: "PostsMapper",
                parameters: [
                  {
                    type: "collection",
                    value:
                      "/sk/mappers/remote/postgres/users/b64_eyJrZXkiOiB7ImNvbCI6ICJpZCIsICJ0eXBlIjogIlNFUklBTCJ9fQ../",
                  },
                  { type: "collection", value: "/sk/mappers/map/ext_6/" },
                ],
              },
            ],
            operator: "map",
          },
        },
        {
          inputs: [],
          name: "/sk/mappers/remote/postgres/posts/b64_eyJrZXkiOiB7ImNvbCI6ICJpZCIsICJ0eXBlIjogIlNFUklBTCJ9fQ../",
          reads: [],
          type: {
            constructors: [
              {
                name: "postgres",
                parameters: [
                  { type: "data", value: "posts" },
                  {
                    type: "data",
                    value: { key: { col: "id", type: "SERIAL" } },
                  },
                ],
              },
            ],
            operator: "remote",
          },
        },
        {
          inputs: [],
          name: "/sk/mappers/remote/postgres/users/b64_eyJrZXkiOiB7ImNvbCI6ICJpZCIsICJ0eXBlIjogIlNFUklBTCJ9fQ../",
          reads: [],
          type: {
            constructors: [
              {
                name: "postgres",
                parameters: [
                  { type: "data", value: "users" },
                  {
                    type: "data",
                    value: { key: { col: "id", type: "SERIAL" } },
                  },
                ],
              },
            ],
            operator: "remote",
          },
        },
        {
          inputs: [
            "/sk/mappers/remote/postgres/upvotes/b64_eyJrZXkiOiB7ImNvbCI6ICJpZCIsICJ0eXBlIjogIlNFUklBTCJ9fQ../",
          ],
          name: "/sk/mappers/map/ext_6/",
          reads: [],
          type: {
            constructors: [{ name: "UpvotesMapper", parameters: [] }],
            operator: "map",
          },
        },
        {
          inputs: [],
          name: "/sk/mappers/remote/postgres/upvotes/b64_eyJrZXkiOiB7ImNvbCI6ICJpZCIsICJ0eXBlIjogIlNFUklBTCJ9fQ../",
          reads: [],
          type: {
            constructors: [
              {
                name: "postgres",
                parameters: [
                  { type: "data", value: "upvotes" },
                  {
                    type: "data",
                    value: { key: { col: "id", type: "SERIAL" } },
                  },
                ],
              },
            ],
            operator: "remote",
          },
        },
      ],
      inputs: [{ alias: "sessions", name: "/sessions/" }],
      outputs: [
        { alias: "postsWithUpvotes", name: "/sk/mappers/map/ext_7/" },
        { alias: "sessions", name: "/sessions/" },
      ],
      reads: [],
    };
    res.status(200).json(graph);
  } catch (e) {
    console.log(e);
    res.status(500).json(e instanceof Error ? e.message : e);
  }
});

app.get("/v1/instances/:resource", (req, res) => {
  try {
    const instances = [
      {
        params: { param: 0 },
        clients: ["c1", "c2"],
      },
      {
        params: { param: 1 },
        clients: ["c3", "c4"],
      },
    ];
    res.status(200).json(instances);
  } catch (e) {
    console.log(e);
    res.status(500).json(e instanceof Error ? e.message : e);
  }
});

const r1 = {
  entities: [
    {
      inputs: ["/cells/"],
      name: "/sk/mappers/map/ext_5/",
      reads: [],
      type: {
        constructors: [
          {
            name: "CallCompute",
            parameters: [
              { type: "collection", value: "/sk/mappers/lazy/ext_4/" },
            ],
          },
        ],
        operator: "map",
      },
    },
  ],
  inputs: [{ alias: "cells", name: "/cells/" }],
  outputs: [{ alias: "output", name: "/sk/mappers/map/ext_5/" }],
  reads: [{ collection: "/sk/mappers/lazy/ext_4/", key: "#tag" }],
};

const r2 = {
  entities: [
    {
      inputs: ["/cells/"],
      name: "/sk/mappers/map/ext_5/",
      reads: ["/sk/mappers/lazy/ext_4/"],
      type: {
        constructors: [
          {
            name: "CallCompute",
            parameters: [
              { type: "collection", value: "/sk/mappers/lazy/ext_4/" },
            ],
          },
        ],
        operator: "map",
      },
    },
    {
      inputs: [],
      name: "/sk/mappers/lazy/ext_4/",
      reads: ["/cells/", "/sk/mappers/lazy/ext_4/"],
      type: {
        constructors: [
          {
            name: "ComputeExpression",
            parameters: [{ type: "collection", value: "/cells/" }],
          },
        ],
        operator: "lazy",
      },
    },
  ],
  inputs: [{ alias: "cells", name: "/cells/" }],
  outputs: [{ alias: "output", name: "/sk/mappers/map/ext_5/" }],
  reads: [{ collection: "/sk/mappers/lazy/ext_4/", key: "#tag" }],
};

app.post("/v1/resource/:resource", (req, res) => {
  try {
    const rsc = req.params.resource;
    const graph = rsc == "r1" ? r1 : rsc == "r2" ? r2 : null;
    if (!graph) res.sendStatus(404);
    else res.status(200).json(graph);
  } catch (e) {
    console.log(e);
    res.status(500).json(e instanceof Error ? e.message : e);
  }
});

app.post("/v1/values", (req, res) => {
  try {
    const entries = [
      ["key0", ["v1", "v2"]],
      ["key1", ["v3", "v4"]],
    ];
    res.status(200).json(entries);
  } catch (e) {
    console.log(e);
    res.status(500).json(e instanceof Error ? e.message : e);
  }
});

function no_cors(req, res, next) {
  res.header("Access-Control-Allow-Credentials", "true");
  res.header("Access-Control-Allow-Methods", "GET,POST,OPTIONS");
  res.header("Access-Control-Allow-Origin", "*");
  if (req.method.toUpperCase() == "OPTIONS") {
    res.statusCode = 204;
    res.setHeader("Content-Length", "0");
    res.end();
  } else {
    next();
  }
}

function debug_no_cors(req, res, next) {
  res.header("Access-Control-Allow-Headers", "Content-Type");
  no_cors(req, res, next);
}

function logger(req, res, next) {
  const start = Date.now();

  res.on("finish", () => {
    const duration = Date.now() - start;
    const log = `[${new Date().toISOString()}] ${req.method} ${req.originalUrl} ${res.statusCode} - ${duration}ms`;
    console.log(log);
  });

  next();
}

app.listen(port, () => {
  console.log(`Skip debug service listening on port ${port.toString()}`);
});
