// import logo from "./logo.svg";
// import "./App.css";
// import DataTable from "./Table/Table";

// function App() {

//   return (
//     <>
//       <DataTable />
//     </>
//   );
// }

// export default App;

import React, { useEffect, useState } from "react";
import axios from "axios";
import DataTable from "./Table/Table";

function App() {
  // State to store the data from the API response

  return (
    <>
     <div className="m-6 mt-4">
     <DataTable/>
     
      </div>
    </>
  );
}

export default App;
