// Requires Node.js and a C++14 compiler. No npm packages are needed.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const { spawnSync } = require('node:child_process');

const repository = path.resolve(__dirname, '..');
const temporaryRoot = fs.mkdtempSync(path.join(os.tmpdir(), 'banking-tests-'));
const executableSuffix = process.platform === 'win32' ? '.exe' : '';
const compiler = process.env.CXX || 'g++';
let checks = 0;

function run(executable, args, options = {}) {
    const result = spawnSync(executable, args, {
        encoding: 'utf8', timeout: 60000, windowsHide: true, ...options,
    });
    if (result.error) throw result.error;
    assert.equal(result.status, options.expectedStatus || 0,
        `${executable} failed:\n${result.stdout}\n${result.stderr}`);
    return result.stdout + result.stderr;
}

function app(name, directory, input, expectedStatus = 0) {
    return run(path.join(temporaryRoot, name + executableSuffix), [], {
        cwd: directory, input, expectedStatus,
    });
}

function includes(output, text) {
    assert.ok(output.includes(text), `Expected ${JSON.stringify(text)} in:\n${output}`);
    ++checks;
}

function testConsole(directory) {
    includes(app('transaction_analyst', directory, '3\n4\n1\n1\n5\n'), 'No transactions found.');
    includes(app('transaction_analyst', directory, '4\n5\n'), 'No deposits found');
    includes(app('transaction_generator', directory, '1\nmissing\n1234\n\n3\n'), 'Incorrect username or PIN.');
    includes(app('customer_manager', directory, 'Alex Morgan\n123\nalex\n1234\n'), 'Initial balance: $10000.00');
    includes(app('customer_manager', directory,
        'Duplicate ID\n123\nother\n1234\nBlair Smith\n456\nblair\n5678\n'),
        'A customer with this national ID already exists.');
    includes(app('customer_manager', directory,
        'Duplicate Name\n789\nalex\n1234\n'), 'This username is already taken.');
    includes(app('customer_manager', directory,
        `${'x'.repeat(70)}\nCasey Jones\nnot a number\n789\ncasey\n1234\n`), 'Enter a valid number.');

    const invalid = app('transaction_generator', directory,
        'oops\n9\n1\nalex\n9999\nalex\n1234\n2\n0\n20000\n20240330\n1\n-5\n20240330\n1\n100\n20240230\n20240330\n0\n3\n');
    for (const text of ['Choose an option from 1 to 3.', 'Incorrect username or PIN.',
        'Choose 1 or 0.', 'Insufficient funds.', 'greater than zero', 'Enter a valid date',
        'New balance: $10100.00']) includes(invalid, text);

    const transactions = app('transaction_generator', directory,
        '1\nalex\n1234\n0\n50\n20240329\n1\n1\n20\n20240328\n1\n1\n30\n20240327\n1\n1\n40\n20240326\n1\n1\n50\n20240325\n0\n3\n');
    includes(transactions, 'New balance: $10190.00');
    const historyBefore = fs.readFileSync(path.join(directory, 'transactions.dat'));
    const reports = app('transaction_analyst', directory,
        '1\n123\nx\nn\np\ne\n2\n123\n20240330\n3\n4\n5\n');
    for (const text of ['Page 1 of 2', 'Page 2 of 2', 'That navigation option is not available.',
        'Deposits: 5 | Total: $240.00', 'Withdrawals: 1 | Total: $50.00', 'Amount: $100.00',
        'Total deposits: $240.00']) includes(reports, text);
    assert.deepEqual(fs.readFileSync(path.join(directory, 'transactions.dat')), historyBefore);
    ++checks;

    includes(app('transaction_generator', directory, '2\n999\n2\n1\n3\n'), 'Transaction not found.');
    includes(app('transaction_generator', directory, '1\nalex\n1234\n1\n10\n20240401\n'),
        'Transaction saved. New balance: $10200.00');
    for (const name of ['transaction_generator', 'transaction_analyst', 'customer_manager']) {
        includes(app(name, directory, ''), 'Input closed. Exiting.');
    }
    // Exactly five items must produce one page, and a sixth must produce two.
    includes(app('transaction_analyst', directory, '1\n123\ne\n5\n'), 'Page 1 of 2');
    app('transaction_generator', directory, '2\n2\n3\n');
    includes(app('transaction_analyst', directory, '1\n123\ne\n5\n'), 'Page 1 of 1');
}

function testWriteFailures(directory) {
    app('customer_manager', directory, 'Alex Morgan\n123\nalex\n1234\n');
    app('transaction_generator', directory, '1\nalex\n1234\n1\n100\n20240330\n0\n3\n');
    const customerPath = path.join(directory, 'customers.dat');
    const transactionPath = path.join(directory, 'transactions.dat');
    const beforeCustomers = fs.readFileSync(customerPath);
    const beforeTransactions = fs.readFileSync(transactionPath);

    // A directory at the temporary-file path reliably blocks writing on each platform.
    for (const blockedPath of [customerPath, transactionPath]) {
        fs.mkdirSync(blockedPath + '.tmp');
        includes(app('transaction_generator', directory,
            '1\nalex\n1234\n1\n10\n20240330\n', 1), 'Cannot open temporary file');
        assert.deepEqual(fs.readFileSync(customerPath), beforeCustomers);
        assert.deepEqual(fs.readFileSync(transactionPath), beforeTransactions);
        checks += 2;
        assert.ok(fs.statSync(blockedPath + '.tmp').isDirectory());
        ++checks;
        fs.rmdirSync(blockedPath + '.tmp');
    }
    fs.writeFileSync(transactionPath, 'truncated');
    includes(app('transaction_analyst', directory, '3\n', 1), 'Incomplete binary record');
}

try {
    for (const name of ['customer_manager', 'transaction_generator', 'transaction_analyst', 'banking_tests']) {
        const source = name === 'banking_tests' ? path.join('tests', name + '.cpp') : name + '.cpp';
        run(compiler, ['-std=c++14', '-Wall', '-Wextra', '-Wpedantic', '-Werror', '-I', repository,
            path.join(repository, 'banking.cpp'), path.join(repository, source),
            '-o', path.join(temporaryRoot, name + executableSuffix)], { cwd: repository });
    }
    const unitDirectory = path.join(temporaryRoot, 'unit-data');
    fs.mkdirSync(unitDirectory);
    process.stdout.write(run(path.join(temporaryRoot, 'banking_tests' + executableSuffix), [unitDirectory]));
    const consoleDirectory = path.join(temporaryRoot, 'console-data');
    fs.mkdirSync(consoleDirectory);
    testConsole(consoleDirectory);
    const failureDirectory = path.join(temporaryRoot, 'failure-data');
    fs.mkdirSync(failureDirectory);
    testWriteFailures(failureDirectory);
    console.log(`${checks} console and file-failure checks passed.`);
} finally {
    fs.rmSync(temporaryRoot, { recursive: true, force: true });
}
