# Banking System

A C++ console application that simulates everyday banking operations: registering customers, recording deposits and withdrawals, updating account balances, and generating transaction reports. Customer accounts and transaction history are saved in local binary files, so they remain available between runs.

Originally built by a team of five for the **Algorithms and Data Structures** course at the **National Technological University (UTN)** in 2024, this project explores file persistence, record processing, sorting, and aggregation. The code and documentation have since been translated into English and refactored to improve reliability and maintainability.

## What it does

The system consists of three applications that share the same data files:

| Application | Features |
| --- | --- |
| Customer manager | Registers a customer with a name, national ID, unique username, and numeric PIN. Each account starts with a balance of $10,000. |
| Transaction generator | Authenticates customers, records deposits and withdrawals, updates balances, and deletes transaction records by ID. |
| Transaction analyst | Lists a customer's transactions in chronological order, shows five records per page, summarizes recent activity, finds the largest transaction, and identifies the customer with the highest total deposits in the reporting period. |

## Build and run

You need a **C++14 compiler** or newer. The manual commands below work with GCC, including MinGW on Windows. No third-party C++ libraries are required.

From the project directory:

```sh
mkdir build
g++ -std=c++14 -Wall -Wextra -Wpedantic banking.cpp customer_manager.cpp -o build/customer_manager
g++ -std=c++14 -Wall -Wextra -Wpedantic banking.cpp transaction_generator.cpp -o build/transaction_generator
g++ -std=c++14 -Wall -Wextra -Wpedantic banking.cpp transaction_analyst.cpp -o build/transaction_analyst
```

On Windows, run the applications from PowerShell:

```powershell
.\build\customer_manager.exe
.\build\transaction_generator.exe
.\build\transaction_analyst.exe
```

On Linux or macOS, use `./build/customer_manager`, `./build/transaction_generator`, and `./build/transaction_analyst`.

Always launch the applications from the **same working directory** so they use the same accounts and transaction history. Data files are created there when needed. Register a customer first, then use the transaction generator to record activity and the analyst to view reports.

### Build with CMake

If you have CMake 3.20 or newer and a configured compiler:

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

With a Visual Studio generator, executables are usually placed in `build/Debug`; run tests with `ctest --test-dir build -C Debug --output-on-failure`. With MinGW, select its generator during configuration: `cmake -S . -B build -G "MinGW Makefiles"`.

## Using the system

1. **Register a customer.** Enter a full name, a positive national ID without separators, a username, and a numeric PIN. Usernames are case-sensitive and accept ASCII letters, digits, and underscores. Names and usernames can each contain up to 49 bytes of text.
2. **Make a transaction.** Sign in with the username and PIN, choose a deposit or withdrawal, and enter a positive amount and a date in `YYYYMMDD` format, such as `20240330`. Withdrawals cannot exceed the account balance. Each successful operation is saved immediately.
3. **View reports.** Select a report in the analyst menu. Customer history uses `N` for the next page, `P` for the previous page, and `E` to return to the menu.

Invalid input prompts you to try again. Closing the input stream exits the application cleanly. Failed file operations report an error and exit with a nonzero status.

### Reporting rules

- Customer history is sorted from oldest to newest. Transactions on the same date retain their recorded order.
- The customer summary uses the date you enter as its reference. It includes transactions from **30 days before that date through the reference date**, including both endpoints, and excludes later transactions.
- The top depositor report uses the **latest date in the transaction history** as its reference, with the same inclusive interval. It compares the total amount deposited, rather than the number of deposits. If totals are equal, the customer with the lowest national ID is shown.
- The largest transaction report considers both deposits and withdrawals across the entire history. If amounts are equal, the first recorded transaction is shown.
- **Deleting a transaction removes its history entry without reversing its effect on the balance.** This preserves the original application's deletion behavior.
- New transaction IDs are one greater than the highest ID still present in the history. Deleting the highest ID allows that number to be reused.

Dates are validated against the calendar, including leap years, and must fall between 1900 and 9999. Dates are entered manually; the applications do not read the system clock for reports.

## Design

| File | Responsibility |
| --- | --- |
| `banking.hpp` | Shared customer and transaction records, report results, and the banking interface. |
| `banking.cpp` | Account operations, binary persistence, date calculations, and reporting. |
| `console.hpp` | Reusable input validation and console formatting. |
| `customer_manager.cpp` | Customer registration flow. |
| `transaction_generator.cpp` | Authentication and transaction menus. |
| `transaction_analyst.cpp` | Report menus and pagination. |
| `tests/` | Banking, compatibility, console, and file-failure regression tests. |

The three applications share one implementation of the account rules and storage routines. Customer methods enforce deposit and withdrawal rules, while transaction methods express customer ownership and date filtering. Standard containers and algorithms replace variable-length arrays, manual field copying, and handwritten sorting. File handles are managed automatically, and repeated interactions use loops instead of recursive calls.

The design applies separation of responsibilities and DRY where they remove duplication, while keeping the small project straightforward. It avoids adding inheritance hierarchies, a database, or a framework that the current features do not need.

## Data files and compatibility

The applications use `customers.dat` and `transactions.dat`. These contain raw binary records, with the same field types and order as the original project. Transactions retain a snapshot of the customer taken before the operation.

If the original `clientes.dat` or `transacciones.dat` files are present, the application automatically renames them to their English equivalents without rewriting their contents. If both an old and a new name exist for the same dataset, it stops with an error so you can choose which files to keep. Back up existing data before opening it with the updated applications.

Raw binary records depend on the compiler's data layout, alignment, architecture, and byte order. **Existing files require a build with a compatible binary layout.** Renaming the files does not convert them between platforms or compiler configurations. The tests verify compatibility with the original record definitions under the compiler used to run them; no historical data files are included in this repository.

Writes use a temporary file before replacing each destination. If saving a balance fails after saving its transaction, the application attempts to restore the previous transaction history. The two files are **not a single atomic transaction**: a crash, power loss, or failed rollback can still leave them inconsistent. Run one application instance at a time; concurrent writes are not supported.

This is an educational simulation. PINs are stored as plain integers, including in transaction snapshots, and leading zeros are not retained. Balances and amounts use `double` to preserve the original record format, so ordinary floating-point rounding applies. There is no encryption, access control for the reporting and deletion menus, or production-grade recovery mechanism.

## Tests

With **Node.js 18 or newer** and GCC available on your path, run:

```sh
node tests/run_tests.js
```

The runner compiles all three applications and the C++ tests with warnings treated as errors, then checks:

- Registration, duplicate detection, authentication, and persisted balances.
- Deposits, withdrawals, insufficient funds, invalid amounts, and overflow.
- Leap years, date boundaries, report totals, sorting, and empty histories.
- Transaction deletion and ID allocation.
- Compatibility with the original binary record definitions and file names.
- Malformed input, end of input, pagination, and truncated records.
- Failed writes and rollback of transaction history when a balance cannot be saved.

All test data and executables are created in a temporary directory and removed afterward. Set the `CXX` environment variable to a GCC-compatible compiler executable if it is not named `g++`.

The CMake test target runs the C++ banking tests; the Node.js runner also exercises the applications through their console interfaces and checks file-write failures.

## Original assignment

The [original course assignment](https://tomasjuliano.notion.site/AyED-Sistema-bancario-24329c864aa6801aa9cfe8c50b0f4c4c) is available in Spanish.
