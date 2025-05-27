# C++ SQL Server

## Overview
This project implements a simple SQL server in C++. It accepts SQL query submissions from clients, processes them, and returns the results. The server is designed to be extensible and can be modified to support additional features as needed.

## Project Structure
```
cpp-sql-server
├── src
│   ├── main.cpp          # Entry point of the application
│   ├── server.cpp        # Implementation of the Server class
│   ├── server.h          # Declaration of the Server class
│   ├── sql_handler.cpp    # Implementation of the SQLHandler class
│   ├── sql_handler.h      # Declaration of the SQLHandler class
│   ├── utils.cpp         # Utility functions
│   └── utils.h          # Declarations of utility functions
├── CMakeLists.txt        # CMake configuration file
├── README.md              # Project documentation
└── tests
    ├── server_test.cpp    # Unit tests for the Server class
    └── sql_handler_test.cpp # Unit tests for the SQLHandler class
```

## Setup Instructions
1. **Clone the repository**:
   ```
   git clone <repository-url>
   cd cpp-sql-server
   ```

2. **Build the project**:
   ```
   mkdir build
   cd build
   cmake ..
   make
   ```

3. **Run the server**:
   ```
   ./cpp-sql-server
   ```

## Usage
Once the server is running, you can submit SQL queries through a client connection. The server will process the queries and return the results.

## Testing
To run the unit tests, navigate to the `tests` directory and execute:
```
./run_tests
```

## Contributing
Contributions are welcome! Please feel free to submit a pull request or open an issue for any enhancements or bug fixes.

## License
This project is licensed under the MIT License. See the LICENSE file for more details.