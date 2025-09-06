#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0
# pylint: disable=cyclic-import
"""Test suite for build_api.py script"""

import contextlib
from io import StringIO
import os
import subprocess
import sys
import tempfile
import unittest

# Add the scripts directory to the path
script_dir = os.path.join(os.path.dirname(__file__), '..', '..', 'scripts')
sys.path.insert(0, script_dir)

# Add the tools directory to the path for u_boot_pylib
tools_dir = os.path.join(os.path.dirname(__file__), '..', '..', 'tools')
sys.path.insert(0, tools_dir)

# pylint: disable=wrong-import-position,import-error
from build_api import rename_function, RenameSymsParser, DeclExtractor
from build_api import ApiGenerator, SymbolRedefiner, main
from u_boot_pylib import tools


class TestBuildApi(unittest.TestCase):
    # pylint: disable=too-many-public-methods
    """Test suite for build_api.py script"""

    def setUp(self):
        """Create temporary files for testing"""
        # pylint: disable=R1732
        self.tmpdir = tempfile.TemporaryDirectory()
        # Create a temp file path for symbols.syms that tests can write to
        self.sympath = os.path.join(self.tmpdir.name, 'symbols.syms')

    def tearDown(self):
        """Clean up temporary files"""
        self.tmpdir.cleanup()

    def write_tmp(self, content, filename):
        """Create a temporary text file with given content"""
        temp_path = os.path.join(self.tmpdir.name, filename)
        tools.write_file(temp_path, content, binary=False)
        return temp_path

    def test_rename_function(self):
        """Test basic function renaming"""
        source_code = '''
/**
 * sprintf() - Format a string and place it in a buffer
 *
 * @buf: The buffer to place the result into
 * @fmt: The format string to use
 * @...: Arguments for the format string
 *
 * The function returns the number of characters written
 * into @buf.
 *
 * See the vsprintf() documentation for format string extensions over C99.
 */
int sprintf(char *buf, const char *fmt, ...)
__attribute__ ((format (__printf__, 2, 3)));
'''
        result = rename_function(source_code, 'sprintf', 'my_sprintf')

        # Check that the function name was changed
        assert 'int my_sprintf(char *buf' in result
        assert 'int sprintf(char *buf' not in result

    def test_rename_sym_parser(self):
        """Test parsing symbol definition file format"""
        content = '''# Test symbols.syms file
file: stdio.h
 printf
 scanf

file: string.h
 strcpy
 strlen=ub_str_length

file: stdlib.h
 malloc=custom_malloc
'''
        tools.write_file(self.sympath, content, binary=False)

        parser = RenameSymsParser(self.sympath)
        renames = parser.parse()

        # Check we got the right number of renames
        assert len(renames) == 5

        # Check default prefix mapping
        printf_rename = next(r for r in renames if r.orig == 'printf')
        assert printf_rename.hdr == 'stdio.h'
        assert printf_rename.new_name == 'ub_printf'

        # Check explicit mapping
        strlen_rename = next(r for r in renames if r.orig == 'strlen')
        assert strlen_rename.hdr == 'string.h'
        assert strlen_rename.new_name == 'ub_str_length'
        malloc_rename = next(r for r in renames if r.orig == 'malloc')
        assert malloc_rename.hdr == 'stdlib.h'
        assert malloc_rename.new_name == 'custom_malloc'

    def test_rename_sym_with_real_file(self):
        """Test parsing with realistic symbols.syms file"""
        symbols_content = '''# Symbols for U-Boot library
file: stdio.h
 printf
 sprintf
 snprintf
 scanf
 sscanf

file: string.h
 memcpy
 memset
 strlen
 strcpy
 strcmp

file: stdlib.h
 malloc
 free
 calloc
'''
        symbols_path = self.write_tmp(symbols_content, 'realistic_symbols.syms')

        parser = RenameSymsParser(symbols_path)

        # Should have some renames
        renames = parser.parse()
        assert renames

        # Check that printf gets renamed to ub_printf
        printf_rename = next((r for r in renames if r.orig == 'printf'), None)
        assert printf_rename is not None
        assert printf_rename.hdr == 'stdio.h'
        assert printf_rename.new_name == 'ub_printf'

    def test_rename_with_parser(self):
        """Test integration between parser and renaming"""
        content = '''file: stdio.h
 sprintf
 printf
'''
        tools.write_file(self.sympath, content, binary=False)
        parser = RenameSymsParser(self.sympath)
        renames = parser.parse()
        # Use the parser results to rename functions in source code
        source_code = '''
int sprintf(char *buf, const char *fmt, ...);
int printf(const char *fmt, ...);
'''
        result = source_code
        for rename in renames:
            result = rename_function(result, rename.orig, rename.new_name)

        # Check that both functions were renamed
        assert 'int ub_sprintf(char *buf' in result
        assert 'int ub_printf(const char *fmt' in result
        assert 'int sprintf(char *buf' not in result
        assert 'int printf(const char *fmt' not in result

    def test_redefine_option(self):
        """Test symbol redefinition in object files"""
        content = '''file: stdio.h
 printf
'''
        rename_syms = self.write_tmp(content, 'redefine_symbols.syms')

        # Create a simple C file with printf (use format string to prevent
        # optimization to puts)
        c_code = '''
#include <stdio.h>
void test_function() {
    printf("%s %d\\n", "Hello", 123);
}
'''
        c_file_path = self.write_tmp(c_code, 'test.c')
        obj_file_path = c_file_path.replace('.c', '.o')
        # obj file will be cleaned up automatically with tmpdir

        # Compile the C file to object file
        compile_cmd = ['gcc', '-c', c_file_path, '-o', obj_file_path]
        subprocess.run(compile_cmd, capture_output=True, text=True, check=True)

        # Check that the object file contains printf symbol
        nm_cmd = ['nm', obj_file_path]
        result = subprocess.run(nm_cmd, capture_output=True, text=True,
                               check=True)
        assert 'printf' in result.stdout

        # Test the parser
        parser = RenameSymsParser(rename_syms)
        renames = parser.parse()

        # Verify we have the expected rename
        assert len(renames) == 1
        assert renames[0].orig == 'printf'
        assert renames[0].new_name == 'ub_printf'
        assert renames[0].hdr == 'stdio.h'

        # Test the actual symbol redefinition
        outfiles, modified = SymbolRedefiner.apply_renames(
            [obj_file_path], renames, self.tmpdir.name, 1)
        assert outfiles
        assert modified == 1  # Should have modified 1 file
        obj_file_path = outfiles[0]  # Use the output file for checking

        # Check that the symbol was renamed
        nm_cmd = ['nm', obj_file_path]
        result = subprocess.run(nm_cmd, capture_output=True, text=True,
                               check=True)

        # Should now have ub_printf instead of printf
        out = result.stdout.replace('ub_printf', '')
        assert 'ub_printf' in result.stdout
        assert 'printf' not in out

    def test_extract_decl(self):
        """Test extracting function declarations from headers"""
        content = '''#ifndef TEST_H
#define TEST_H

/**
 * sprintf() - Format a string and place it in a buffer
 *
 * @buf: The buffer to place the result into
 * @fmt: The format string to use
 * @...: Arguments for the format string
 *
 * The function returns the number of characters written
 * into @buf.
 */
int sprintf(char *buf, const char *fmt, ...)
\t\t__attribute__ ((format (__printf__, 2, 3)));

// Another function without detailed comment

int printf(const char *fmt, ...);

/**
 * strlen() - Calculate the length of a string
 * @s: The string to measure
 *
 * Return: The length of the string
 */

size_t strlen(const char *s);

/* Broken comment block - ends without proper start */
*/
#define SOME_MACRO 1
int broken_comment_func(void);

/* Normal function preceded by non-comment content */
int other_content_func(void);

#endif
'''
        hdr = self.write_tmp(content, 'test.h')
        # Test finding sprintf with comment
        decl = DeclExtractor.extract_decl(hdr, 'sprintf')
        assert decl is not None
        expected = '''/**
 * sprintf() - Format a string and place it in a buffer
 *
 * @buf: The buffer to place the result into
 * @fmt: The format string to use
 * @...: Arguments for the format string
 *
 * The function returns the number of characters written
 * into @buf.
 */
int sprintf(char *buf, const char *fmt, ...)
\t\t__attribute__ ((format (__printf__, 2, 3)));'''
        assert decl == expected, (
            f'Expected:\n{expected}\n\nGot:\n{decl}')

        # Test finding printf without detailed comment
        decl = DeclExtractor.extract_decl(hdr, 'printf')
        assert decl is not None
        expected = '''// Another function without detailed comment

int printf(const char *fmt, ...);'''
        assert decl == expected, (
            f'Expected:\n{expected}\n\nGot:\n{decl}')

        # Test finding strlen with comment
        strlen_decl = DeclExtractor.extract_decl(hdr, 'strlen')
        assert strlen_decl is not None
        expected_strlen = '''/**
 * strlen() - Calculate the length of a string
 * @s: The string to measure
 *
 * Return: The length of the string
 */

size_t strlen(const char *s);'''
        assert strlen_decl == expected_strlen, (
            f'Expected:\n{expected_strlen}\n\nGot:\n{strlen_decl}')

        # Test function not found
        assert not DeclExtractor.extract_decl(hdr, 'nonexistent')

        # Test function with broken comment block (should return None)
        broken_decl = DeclExtractor.extract_decl(hdr, 'broken_comment_func')
        assert broken_decl is not None
        assert 'int broken_comment_func(void);' in broken_decl

        # Test function preceded by non-comment content (no comment)
        other_decl = DeclExtractor.extract_decl(hdr, 'other_content_func')
        assert other_decl is not None
        assert 'int other_content_func(void);' in other_decl

    def test_extract_decl_malformed_comment(self):
        """Test extracting declaration with malformed comment block"""
        # Create header where */ appears but no /** is found backwards
        content = '''#ifndef TEST_H
#define TEST_H

some code here
*/
int malformed_func(void);

#endif
'''
        hdr = self.write_tmp(content, 'malformed.h')

        # This should find the function but no comment (malformed comment)
        decl = DeclExtractor.extract_decl(hdr, 'malformed_func')
        assert decl is not None
        assert decl == 'int malformed_func(void);'

    def test_symbol_redefiner_coverage(self):
        """Test SymbolRedefiner edge cases for better coverage"""
        content = '''file: stdio.h
 printf
 custom_func
'''
        rename_syms = self.write_tmp(content, 'coverage_symbols.syms')

        # Create C file with defined symbol (not just undefined reference)
        c_code_defined = '''
void printf(const char *fmt, ...) {
    // Custom printf implementation
}
'''
        c_file_defined = self.write_tmp(c_code_defined, 'defined_symbol.c')
        obj_file_defined = c_file_defined.replace('.c', '.o')

        # Compile to create object with defined symbol
        compile_cmd = ['gcc', '-c', c_file_defined, '-o', obj_file_defined]
        subprocess.run(compile_cmd, capture_output=True, text=True, check=True)

        # Create C file with no target symbols at all
        c_code_no_symbols = '''
void other_func(void) {
    int x = 42;
}
'''
        c_file_no_symbols = self.write_tmp(c_code_no_symbols, 'no_symbols.c')
        obj_file_no_symbols = c_file_no_symbols.replace('.c', '.o')

        compile_cmd = ['gcc', '-c', c_file_no_symbols, '-o',
                       obj_file_no_symbols]
        subprocess.run(compile_cmd, capture_output=True, text=True, check=True)

        # Test with both files
        parser = RenameSymsParser(rename_syms)
        renames = parser.parse()

        # This should process both files - one with defined symbol, one without
        # target symbols
        # Test with verbose output
        stdout = StringIO()
        with contextlib.redirect_stdout(stdout):
            outfiles, modified = SymbolRedefiner.apply_renames(
                [obj_file_defined, obj_file_no_symbols], renames,
                self.tmpdir.name, 1, verbose=True)

        assert outfiles
        assert len(outfiles) == 2
        # Should have modified 1 file (the one with defined symbol)
        assert modified == 1
        assert 'Copied and modified' in stdout.getvalue()

    def test_apply_renames_empty_symbols(self):
        """Test SymbolRedefiner.apply_renames with empty symbol list"""
        # Create a simple object file
        c_code = '''
void test_func(void) {
    int x = 42;
}
'''
        c_file = self.write_tmp(c_code, 'test_empty_syms.c')
        obj_file = c_file.replace('.c', '.o')

        compile_cmd = ['gcc', '-c', c_file, '-o', obj_file]
        subprocess.run(compile_cmd, capture_output=True, text=True, check=True)

        # Call apply_renames with empty symbol list
        empty_syms = []
        obj_files = [obj_file]
        result_files, modified = SymbolRedefiner.apply_renames(
            obj_files, empty_syms, self.tmpdir.name, 1)

        # Should return the original obj_files unchanged and 0 modified
        assert result_files == obj_files
        assert modified == 0

    def test_api_generation_empty_symbols(self):
        """Test API generation with empty symbol list"""
        api_file = self.write_tmp('', 'empty_api.h')

        # Test generate_hdr with empty symbol list
        stderr = StringIO()
        with contextlib.redirect_stderr(stderr):
            result = ApiGenerator.generate_hdr([], '/nonexistent', api_file)

        # Should return 0 and print warning
        assert result == 0
        assert 'Warning: No symbols found' in stderr.getvalue()

    def test_parse_args_errors(self):
        """Test main() with parse_args validation errors"""

        # Test 1: --redefine with no object files
        test_args = ['test.syms', '--redefine', '--output-dir', '/tmp']

        stderr = StringIO()
        with contextlib.redirect_stderr(stderr):
            result = main(test_args)

        assert result == 1
        assert 'Error: --redefine requires at least one object file' in \
            stderr.getvalue()

        # Test 2: --redefine without --output-dir
        test_args = ['test.syms', '--redefine', 'test.o']

        stderr = StringIO()
        with contextlib.redirect_stderr(stderr):
            result = main(test_args)

        assert result == 1
        assert 'Error: --output-dir is required with --redefine' in \
            stderr.getvalue()

        # Test 3: --api without --include-dir
        test_args = ['test.syms', '--api', 'api.h']

        stderr = StringIO()
        with contextlib.redirect_stderr(stderr):
            result = main(test_args)

        assert result == 1
        assert 'Error: --include-dir is required with --api' in stderr.getvalue()

    def test_main_function_paths(self):
        """Test main function with different argument combinations"""

        # Create test files
        content = '''file: stdio.h
 printf
'''
        rename_syms = self.write_tmp(content, 'rename.syms')

        c_code = '''
#include <stdio.h>
void test_function() {
    printf("%s\\n", "test");
}
'''
        c_file = self.write_tmp(c_code, 'main_test.c')
        obj_file = c_file.replace('.c', '.o')

        compile_cmd = ['gcc', '-c', c_file, '-o', obj_file]
        subprocess.run(compile_cmd, capture_output=True, text=True, check=True)

        # Test redefine path
        test_args = [rename_syms, '--redefine', obj_file, '--output-dir',
                     self.tmpdir.name, '--verbose']
        stdout = StringIO()
        stderr = StringIO()
        with (contextlib.redirect_stdout(stdout),
              contextlib.redirect_stderr(stderr)):
            result = main(test_args)
        assert result == 0

        # Check that timing message was printed to stderr with verbose
        stderr = stderr.getvalue()
        assert 'Processed 1 files (0 modified) in' in stderr

    def test_main_function_with_jobs(self):
        """Test main function with --jobs option to exercise max_workers path"""

        # Create test files
        content = '''file: stdio.h
 printf
'''
        rename_syms = self.write_tmp(content, 'rename.syms')

        c_code = '''
#include <stdio.h>
void test_function() {
    printf("%s\\n", "test");
}
'''
        c_file = self.write_tmp(c_code, 'jobs_test.c')
        obj_file = c_file.replace('.c', '.o')

        compile_cmd = ['gcc', '-c', c_file, '-o', obj_file]
        subprocess.run(compile_cmd, capture_output=True, text=True, check=True)

        # Test redefine path with explicit --jobs option
        test_args = [rename_syms, '--redefine', obj_file, '--output-dir',
                     self.tmpdir.name, '--jobs', '2', '--verbose']
        stdout = StringIO()
        stderr = StringIO()
        with (contextlib.redirect_stdout(stdout),
              contextlib.redirect_stderr(stderr)):
            result = main(test_args)
        assert result == 0

        # Check that timing message includes thread count
        stderr = stderr.getvalue()
        assert 'Processed 1 files (0 modified) in' in stderr

        # Test API generation path with verbose output
        fake_stdio = '''#ifndef STDIO_H
#define STDIO_H
int printf(const char *fmt, ...);
#endif
'''
        self.write_tmp(fake_stdio, 'stdio.h')
        api_file = self.write_tmp('', 'main_api.h')

        test_args = [rename_syms, '--api', api_file, '--include-dir',
                     self.tmpdir.name, '--output-dir', self.tmpdir.name,
                     '--verbose']

        stdout = StringIO()
        with contextlib.redirect_stdout(stdout):
            result = main(test_args)

        assert result == 0
        assert 'Generated API header:' in stdout.getvalue()

    def test_main_api_generation_failure(self):
        """Test main() when API generation fails"""

        # Create test files that will cause API generation to fail
        content = '''file: nonexistent.h
 missing_function
'''
        rename_syms = self.write_tmp(content, 'failing_api.syms')
        api_file = self.write_tmp('', 'failing_api.h')

        # This will fail because nonexistent.h doesn't exist
        test_args = [rename_syms, '--api', api_file, '--include-dir',
                    '/nonexistent_dir', '--output-dir', self.tmpdir.name]

        stderr = StringIO()
        with contextlib.redirect_stderr(stderr):
            result = main(test_args)

        # Should return 1 because API generation failed
        assert result == 1
        assert 'Missing header files:' in stderr.getvalue()

    def test_api_generation(self):
        """Test API header generation"""
        content = '''file: stdio.h
 printf
'''
        tools.write_file(self.sympath, content, binary=False)

        api = self.write_tmp('', 'api.h')
        parser = RenameSymsParser(self.sympath)
        renames = parser.parse()

        # Generate the API header - this will fail since stdio.h is not found
        captured = StringIO()
        with contextlib.redirect_stderr(captured):
            result = ApiGenerator.generate_hdr(renames, '/nonexistent', api)

        # This test expects failure since stdio.h header is not available
        assert result == 1

    def test_api_generation_missing_headers(self):
        """Test API generation error handling for missing header files"""
        content = '''file: nonexistent.h
 missing_func
'''
        tools.write_file(self.sympath, content, binary=False)

        api = self.write_tmp('', 'api.h')
        parser = RenameSymsParser(self.sympath)
        renames = parser.parse()

        # This should exit with an error
        captured = StringIO()
        with contextlib.redirect_stderr(captured):
            result = ApiGenerator.generate_hdr(renames, '/nonexistent', api)
            assert result == 1, f'Expected return code 1, got {result}'

        assert 'Missing header files:' in captured.getvalue()
        assert 'nonexistent.h' in captured.getvalue()

    def test_api_generation_missing_functions(self):
        """Test API generation error handling for missing functions"""
        # Create a fake stdio.h with a different function for testing
        fake_stdio_content = '''#ifndef STDIO_H
#define STDIO_H
int existing_func(void);
#endif
'''
        self.write_tmp(fake_stdio_content, 'stdio.h')
        include_dir = self.tmpdir.name

        content = '''file: stdio.h
 nonexistent_function
'''
        tools.write_file(self.sympath, content, binary=False)

        api = self.write_tmp('', 'api.h')
        parser = RenameSymsParser(self.sympath)
        renames = parser.parse()

        # This should exit with an error for missing function declarations
        captured = StringIO()
        with contextlib.redirect_stderr(captured):
            result = ApiGenerator.generate_hdr(renames, include_dir, api)
            assert result == 1, f'Expected return code 1, got {result}'

        assert 'Missing function declarations:' in captured.getvalue()
        assert 'nonexistent_function in stdio.h' in captured.getvalue()

    def test_parser_exceptions(self):
        """Test parser error handling for invalid formats"""

        # Test 1: Symbol without header file
        inval1 = '''# Test file with symbol before header
 printf
file: stdio.h
 scanf
'''
        temp_path1 = self.write_tmp(inval1, 'test1.syms')
        parser = RenameSymsParser(temp_path1)
        with self.assertRaises(ValueError) as cm:
            parser.parse()
        self.assertIn("Symbol 'printf' found without a header file directive",
                      str(cm.exception))

        # Test 2: Invalid format (non-indented, non-file line)
        inval2 = '''file: stdio.h
 printf
invalid_line_here
 scanf
'''
        temp_path2 = self.write_tmp(inval2, 'test2.syms')
        parser = RenameSymsParser(temp_path2)
        with self.assertRaises(ValueError) as cm:
            parser.parse()
        self.assertIn("Invalid format - symbols must be indented",
                      str(cm.exception))

    def test_main_dump_symbols(self):
        """Test main function with dump option"""
        content = '''file: stdio.h
 printf
 sprintf

file: string.h
 strlen
'''
        rename_syms = self.write_tmp(content, 'test_symbols.syms')

        # Mock sys.argv to simulate command line arguments
        original_argv = sys.argv
        try:
            sys.argv = ['build_api.py', rename_syms, '--dump']

            # Capture stdout to check dump output
            captured = StringIO()
            with contextlib.redirect_stdout(captured):
                result = main()

            assert result == 0
            output = captured.getvalue()
            assert all(item in output for item in
                       ['printf', 'sprintf', 'strlen', 'stdio.h', 'string.h'])

        finally:
            sys.argv = original_argv


if __name__ == "__main__":
    unittest.main()
