#!/bin/bash
# Test AES256 encryption functionality

echo "Testing AES256 encryption for minigit..."
echo

# Clean up any previous tests
killall minigit 2>/dev/null || true
rm -rf encryption_test_server encryption_test_client
sleep 1

# Create test directories
mkdir -p encryption_test_server
mkdir -p encryption_test_client

# Step 1: Initialize test repositories
echo "Step 1: Setting up test repositories..."
cd encryption_test_server
../build/bin/minigit init
echo "Test file content" > test.txt
../build/bin/minigit add test.txt
../build/bin/minigit commit -m "Initial test commit"
cd ..

cd encryption_test_client
../build/bin/minigit init
echo "remote=server://localhost:8080/test_repo" > .minigit/config
cd ..

# Step 2: Start server with password (enabling encryption)
echo "Step 2: Starting server with password authentication (encryption enabled)..."
build/bin/minigit server --port 8080 --root encryption_test_server --password "testpass123" &
SERVER_PID=$!
sleep 2

# Step 3: Test encrypted operations
echo "Step 3: Testing encrypted authentication and operations..."

cd encryption_test_client

# Test 1: Authentication (should enable encryption)
echo "  - Testing authentication with password..."
../build/bin/minigit connect --host localhost --port 8080 --password "testpass123" <<EOF
login
exit
EOF

if [ $? -eq 0 ]; then
    echo "    ✓ Authentication successful"
else
    echo "    ✗ Authentication failed"
fi

# Test 2: List repositories (should use encryption)
echo "  - Testing list repositories with encryption..."
../build/bin/minigit connect --host localhost --port 8080 --password "testpass123" <<EOF
login
list
exit
EOF

if [ $? -eq 0 ]; then
    echo "    ✓ List repositories successful"
else
    echo "    ✗ List repositories failed"
fi

cd ..

# Step 4: Test without password (no encryption)
echo "Step 4: Testing without password (no encryption)..."
killall minigit 2>/dev/null || true
sleep 1

# Start server without password
build/bin/minigit server --port 8080 --root encryption_test_server &
SERVER_PID=$!
sleep 2

cd encryption_test_client

echo "  - Testing authentication without password..."
../build/bin/minigit connect --host localhost --port 8080 <<EOF
list
exit
EOF

if [ $? -eq 0 ]; then
    echo "    ✓ Non-encrypted connection successful"
else
    echo "    ✗ Non-encrypted connection failed"
fi

cd ..

# Cleanup
echo
echo "Cleaning up..."
killall minigit 2>/dev/null || true
rm -rf encryption_test_server encryption_test_client

echo "Encryption test completed!"