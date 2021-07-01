import {Request, Response} from 'express';
import { QueryError, RowDataPacket, FieldPacket } from 'mysql2';
import { stringify } from 'qs';

const express = require('express');
const http = require('http');
const path = require('path')
const mysql = require('mysql2/promise');    // Use mysql2 bc https://stackoverflow.com/a/56509065

const app = express();
const port = process.env.PORT || 3000

const dbConfig = {    
    host: 'localhost',
    user: 'laundry_backend',
    password: 'admin'
}

// Go back (..) twice because the server is run through server.js in the /build directory
app.use('/', express.static(path.join(__dirname, '..', '..', 'front-end', 'dist', 'LaundrySensorSite')));

app.get('/api', (req: Request, res: Response) => {
    queryServer(1).then((sensor) => {
        res.status(200).send({message: `Hi ${sensor}`});
    });
});

// TODO: add connection pooling for higher efficiency
// TODO: SSL implementation to allow for HTTPS certification
// TODO: Decide how to best open/close connection
async function queryServer(id: number): Promise<string> {
    const query='SELECT (sensor_name) FROM laundrydb.sensors WHERE (sensor_id)=?;'
    
    // Create the connection
    const conn = await mysql.createConnection(dbConfig);

    // Essentially just passes any errors that might have occurred when creating the connection above
    await conn.connect(function(err: Error) {
        if (err) throw err;
        console.log("Connected to LaundryDB!");
    });

    // Using query in form of: query(sqlString, values, callback) to prevent injection attacks
    //  internally, it runs connection.escape() and replaces all ?'s with appropriate values
    const [rows, fields] = await conn.execute(query, [id]);

    const sensorName = rows[0]['sensor_name'];

    await conn.end((err: Error) => {
        if (err) return console.log('Error: ' + err.message);
        console.log('Closed the laundrydb connection');
    });

    return sensorName;
}

const server = http.createServer(app);
server.listen(port, () => console.log(`Server listening on port ${port}`));