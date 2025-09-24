#!/usr/bin/env python3
"""
Main test runner for RAM project
Runs all test suites: unit, integration, performance, stress, e2e
"""

import sys
import os
import subprocess
import argparse
from pathlib import Path

def run_tests(test_type, verbose=False):
    """Run specific test type"""
    test_dir = Path(__file__).parent / test_type
    
    if not test_dir.exists():
        print(f"ERROR: Test directory {test_dir} does not exist")
        return False
    
    print(f"Running {test_type} tests...")
    
    # Find all test files
    test_files = list(test_dir.glob("test_*.py"))
    
    if not test_files:
        print(f"WARNING: No test files found in {test_dir}")
        return True
    
    # Run tests with pytest
    cmd = ["python3", "-m", "pytest", "-v" if verbose else ""] + [str(f) for f in test_files]
    cmd = [c for c in cmd if c]  # Remove empty strings
    
    try:
        result = subprocess.run(cmd, cwd=test_dir.parent, capture_output=True, text=True)
        if result.returncode == 0:
            print(f"PASS: {test_type} tests passed")
            return True
        else:
            print(f"FAIL: {test_type} tests failed")
            print(result.stdout)
            print(result.stderr)
            return False
    except Exception as e:
        print(f"ERROR: Error running {test_type} tests: {e}")
        return False

def main():
    parser = argparse.ArgumentParser(description="Run RAM project tests")
    parser.add_argument("--type", choices=["unit", "integration", "performance", "stress", "e2e", "all"], 
                       default="all", help="Test type to run")
    parser.add_argument("-v", "--verbose", action="store_true", help="Verbose output")
    
    args = parser.parse_args()
    
    print("RAM Project Test Suite")
    print("=" * 50)
    
    if args.type == "all":
        test_types = ["unit", "integration", "performance", "stress", "e2e"]
    else:
        test_types = [args.type]
    
    success = True
    for test_type in test_types:
        if not run_tests(test_type, args.verbose):
            success = False
    
    if success:
        print("\nSUCCESS: All tests passed!")
        sys.exit(0)
    else:
        print("\nFAILURE: Some tests failed!")
        sys.exit(1)

if __name__ == "__main__":
    main()
