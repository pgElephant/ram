#!/usr/bin/env python3
"""
PGRaft Unit Tests
Copyright (c) 2024-2025, pgElephant, Inc.
"""

import unittest
import psycopg2
import time
import subprocess
import os
import sys

class PGRaftUnitTests(unittest.TestCase):
    """Unit tests for PGRaft extension"""
    
    @classmethod
    def setUpClass(cls):
        """Set up test class"""
        cls.conn = None
        cls.cursor = None
        
    def setUp(self):
        """Set up each test"""
        try:
            self.conn = psycopg2.connect(
                host='localhost',
                port=5432,
                user='postgres',
                password='postgres',
                database='postgres'
            )
            self.cursor = self.conn.cursor()
        except Exception as e:
            self.skipTest(f"Could not connect to PostgreSQL: {e}")
    
    def tearDown(self):
        """Clean up after each test"""
        if self.cursor:
            self.cursor.close()
        if self.conn:
            self.conn.close()
    
    def test_extension_exists(self):
        """Test that PGRaft extension exists"""
        self.cursor.execute("SELECT * FROM pg_extension WHERE extname = 'pgraft'")
        result = self.cursor.fetchone()
        self.assertIsNotNone(result, "PGRaft extension not found")
    
    def test_pgraft_init(self):
        """Test pgraft_init function"""
        self.cursor.execute("SELECT pgraft_init()")
        result = self.cursor.fetchone()
        self.assertTrue(result[0], "pgraft_init failed")
    
    def test_pgraft_status(self):
        """Test pgraft_status function"""
        self.cursor.execute("SELECT pgraft_status()")
        result = self.cursor.fetchone()
        self.assertIsNotNone(result, "pgraft_status returned null")
    
    def test_pgraft_is_healthy(self):
        """Test pgraft_is_healthy function"""
        self.cursor.execute("SELECT pgraft_is_healthy()")
        result = self.cursor.fetchone()
        self.assertIsNotNone(result, "pgraft_is_healthy returned null")
    
    def test_pgraft_is_leader(self):
        """Test pgraft_is_leader function"""
        self.cursor.execute("SELECT pgraft_is_leader()")
        result = self.cursor.fetchone()
        self.assertIsNotNone(result, "pgraft_is_leader returned null")
    
    def test_pgraft_get_nodes(self):
        """Test pgraft_get_nodes function"""
        self.cursor.execute("SELECT pgraft_get_nodes()")
        result = self.cursor.fetchone()
        self.assertIsNotNone(result, "pgraft_get_nodes returned null")
    
    def test_pgraft_get_leader(self):
        """Test pgraft_get_leader function"""
        self.cursor.execute("SELECT pgraft_get_leader()")
        result = self.cursor.fetchone()
        self.assertIsNotNone(result, "pgraft_get_leader returned null")
    
    def test_pgraft_set_debug(self):
        """Test pgraft_set_debug function"""
        self.cursor.execute("SELECT pgraft_set_debug(true)")
        result = self.cursor.fetchone()
        self.assertTrue(result[0], "pgraft_set_debug failed")
        
        self.cursor.execute("SELECT pgraft_set_debug(false)")
        result = self.cursor.fetchone()
        self.assertTrue(result[0], "pgraft_set_debug failed")

if __name__ == '__main__':
    unittest.main()
