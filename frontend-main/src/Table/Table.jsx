// import * as React from "react";
import { Button } from "@mui/material";
import Paper from "@mui/material/Paper";
import Table from "@mui/material/Table";
import TableBody from "@mui/material/TableBody";
import TableCell from "@mui/material/TableCell";
import TableContainer from "@mui/material/TableContainer";
import TableHead from "@mui/material/TableHead";
import TablePagination from "@mui/material/TablePagination";
import TableRow from "@mui/material/TableRow";
import { ExportJsonCsv } from "react-export-json-csv";

import axios from "axios";

import React, { useEffect, useState } from "react";
import { mkConfig, generateCsv, download } from "export-to-csv";

const columns = [
  { id: "device_name", label: "NAME", minWidth: 50 },
  { id: "created_at", label: "TIMESTAMP", minWidth: 100 },
  { id: "rssi", label: "RSSI (dB)", minWidth: 100 },
  { id: "bridge", label: "BRIDGE", minWidth: 100 },
  { id: "distance", label: "Distance (m)", minWidth: 100 },
];


const csv_headers = [
  {
    key: "device_name",
    name: "NAME",
  },
  {
    key: "created_at",
    name: "TIMESTAMP",
  },

  {
    key: "rssi",
    name: "RSSI (db)",
  },

  {
    key: "bridge",
    name: "BRIDGE",
  },
  {
    key: "distance",
    name: "DISTANCE(m)",
  },
];

const dataz = [
  {
    id: "1",
    fname: "John",
  },
  {
    id: "2",
    fname: "Doe",
  },
];

function handleDownloadData(data) {}

export default function DataTable() {
  const [page, setPage] = React.useState(0);
  const [data, setData] = useState([]);
  const [rows, setRows] = useState([]);
  const [rowsPerPage, setRowsPerPage] = React.useState(10);

  useEffect(() => {
    // Define the function to make the GET request
    const fetchData = async () => {
      try {
        // Make a GET request using Axios
        //const response = await axios.get("http://178.128.162.43:4002/alldata");
        //const response = await axios.get("https://data-ble-backend.onrender.com/alldata");
        const response = await axios.get(
         // "http://192.168.100.43:8080/get-data"

          "https://new-backend-c3hj.onrender.com/get-data"

          // "https://mdot-backend.onrender.com/retrieveData"
        );

        response.data.sort(
          (a, b) => new Date(b.created_at) - new Date(a.created_at)
        );

        // Set the data in the state
        setRows(response.data);
        console.log("response", response.data);
      } catch (error) {
        // Handle errors if the request fails
        console.error("Error fetching data:", error);
      }
    };

    // Call the fetch data function
    fetchData();
  }, []); // The empty dependency array ensures that this effect runs once after the initial render

  const handleChangePage = (event, newPage) => {
    setPage(newPage);
  };

  const handleChangeRowsPerPage = (event) => {
    setRowsPerPage(+event.target.value);
    setPage(0);
  };

  return (
    <>
      {" "}
      <Paper sx={{ width: "100%", overflow: "hidden" }}>
        <TableContainer sx={{ maxHeight: 440 }}>
          <Table stickyHeader aria-label="sticky table">
            <TableHead>
              <TableRow>
                {columns.map((column) => (
                  <TableCell
                    key={column.id}
                    align={column.align}
                    style={{ minWidth: column.minWidth }}
                  >
                    {column.label}
                  </TableCell>
                ))}
              </TableRow>
            </TableHead>
            <TableBody>
              {rows
                .slice(page * rowsPerPage, page * rowsPerPage + rowsPerPage)
                .map((row) => {
                  return (
                    <TableRow hover role="checkbox" tabIndex={-1} key={row.ID}>
                      {columns.map((column) => {
                        const value = row[column.id];
                        return (
                          <TableCell key={column.id} align={column.align}>
                            {column.format && typeof value === "number"
                              ? column.format(value)
                              : value}
                          </TableCell>
                        );
                      })}
                    </TableRow>
                  );
                })}
            </TableBody>
          </Table>
        </TableContainer>
        <TablePagination
          rowsPerPageOptions={[10, 25, 100]}
          component="div"
          count={rows.length}
          rowsPerPage={rowsPerPage}
          page={page}
          onPageChange={handleChangePage}
          onRowsPerPageChange={handleChangeRowsPerPage}
        />
      </Paper>
      {/* <Button variant="contained" onClick={() => download(csvConfig)(csv)}>
        Download csv
      </Button> */}
      <ExportJsonCsv headers={csv_headers} items={rows}>
        <Button variant="contained">Download csv</Button>
      </ExportJsonCsv>
    </>
  );
}
