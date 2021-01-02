/*
Binary Resources for C++ Compiler (BRCXXC) version 2.1.2
Copyright 2019, 2021 Ryan P. Nicholl <exaeta@protonmail.com>

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#include <filesystem>
#include <fstream>
#include <sstream>
#include <locale>
#include <memory>
#include <iostream>
#include <set>
#include <vector>
#include <utility>
#include <optional>


struct fi
{
  std::vector<std::string> m_sym;
  std::string m_head_decl;
  std::string m_full_decl;
  std::optional<std::string> m_file_path;
  bool m_export_dll = false;
  bool m_import_dll = false;
  bool m_char = false;
};

int main(int argc, char ** argv)
{
    try {
        int i = 1;

        enum class mode {
            header, object, fullfill
        };

        bool implementation = false;


        mode md;
        bool export_syms = false;
        bool import_syms = false;
        st:
        if (i >= argc) {
            std::cerr << "Unexpected end of argument list" << std::endl;
            return -1;
        }
        std::string execution_type = std::string(argv[i++]);
        if (execution_type == "DLLEXPORT_ALL" && export_syms == false) {
            export_syms = true;
            goto st;
        }
        if (execution_type == "DLLIMPORT_ALL" && export_syms == false) {
            import_syms = true;
            goto st;
        }

        if (execution_type == "HEADER") {
            md = mode::header;
        } else if (execution_type == "OBJECT") {
            md = mode::object;
        } else if (execution_type == "IMPLEMENTATION_HEADER") {
            md = mode::header;
            implementation = true;
        } else if (false && execution_type == "FULLFILL") {
            md = mode::fullfill;
            i++;
            if (i >= argc) {
                std::cerr << "Unexpected end of argument list after FULLFILL" << std::endl;
                return -1;
            }
        } else {
            std::cerr << "brcxxc: Expected OBJECT or HEADER" << std::endl;
            return -2;
        }

        std::string guard;


        if (md == mode::header) {


            if (i >= argc) {
                std::cerr << "brcxxc: Expected header guard" << std::endl;
                return -2;
            }
            guard = argv[i++];


            for (char &c : guard) {
                if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c == '_') || (c >= '0' && c <= '9'))) {
                    c = '_';
                }
            }


            if (guard.empty()) {

                return -3;
            }

            if (guard[0] >= '0' && guard[0] <= '9') {
                std::cerr << "brcxxc: Illegal header guard, identifier cannot start with numbers" << std::endl;

                return -4;
            }

            if (!implementation) {
                if (guard[0] == '_') {
                    std::cerr
                            << "brcxxc: Illegal header guard, not running in IMPLMENTATION_HEADER mode, see ISO C++ 17.4.3.1.2"
                            << std::endl;

                    return -4;
                }

                for (int i = 1; i < guard.size(); i++) {
                    if (guard[i] == '_' && guard[i - 1] == '_') {
                        std::cerr
                                << "brcxxc: Illegal header guard, not running in IMPLMENTATION_HEADER mode, see ISO C++ 17.4.3.1.2"
                                << std::endl;
                        return -4;
                    }
                }
            }

        }


        if (i >= argc) {
            std::cerr << "brcxxc: Expected output file" << std::endl;
            return -2;
        }
        std::string output_file = argv[i++];


        std::set<std::string> includes;

        std::vector<fi> files;

        for (i; i < argc;) {
            bool dll_export = false;
            bool dll_import = false;
            fi f;


            // When in header mode, we don't need to get the filename unless we need to know the size of the file

            st2:


            if (i >= argc) {
                std::cerr << "Expected <type> field or DLLEXPORT / DLLIMPORT" << std::endl;
                return EXIT_FAILURE;
            }

            if (argv[i] == "DLLEXPORT" && dll_export == false) {
                dll_export = true;
                i++;
            } else if (argv[i] == "DLLIMPORT" && dll_import == false) {
                dll_import = true;
                i++;
            }

            if (i >= argc) {
                std::cerr << "Expected <type> field" << std::endl;
                return EXIT_FAILURE;
            }

            std::string type = argv[i++];
            if (i >= argc) {
                std::cerr << "Expected <symbol> field" << std::endl;
                return EXIT_FAILURE;
            }
            std::string symbol = argv[i++];

            std::string full_symbol = symbol;
            std::string short_symbol;


            if (export_syms == true) dll_export = true;
            if (import_syms == true) dll_import = true;

            std::optional<std::string> filename;

            auto get_filename = [&] {
                if (filename.has_value()) return;
                if (i >= argc) {
                    std::cerr << "Expected <type> field" << std::endl;
                    throw EXIT_FAILURE;
                }
                filename = argv[i++];
            };


            {
                std::vector<std::string> symparts;


                for (size_t at; (at = symbol.find("::")) != std::string::npos; at) {
                    symparts.push_back(symbol.substr(0, at));
                    symbol = symbol.substr(at + 2);
                }
                short_symbol = std::move(symbol);
                f.m_sym = std::move(symparts);
            }

            if (type == "STRING") {
                includes.insert("<string>");

                f.m_head_decl = "std::string const " + short_symbol + ";";
                f.m_full_decl = "std::string const " + full_symbol;
                f.m_char = true;
            } else if (type == "BYTE_VECTOR") {
                f.m_head_decl = "std::vector<std::byte> const " + short_symbol + ";";
                f.m_full_decl = "std::vector<std::byte> const " + full_symbol;

                includes.insert("<vector>");
                includes.insert("<cstddef>");
            } else if (type == "CHAR_VECTOR") {
                f.m_head_decl = "std::vector<char> const " + short_symbol + ";";
                f.m_full_decl = "std::vector<char> const " + full_symbol;
                f.m_char = true;
                includes.insert("<vector>");
            } else if (type == "U8_VECTOR") {
                f.m_head_decl = "std::vector<std::uint8_t> const " + short_symbol + ";";
                f.m_full_decl = "std::vector<std::uint8_t> const " + full_symbol;
                includes.insert("<vector>");
                includes.insert("<cinttypes>");
            } else if (type == "C_CHAR_ARRAY") {
                get_filename();
                std::size_t sz = std::filesystem::file_size(filename.value());
                std::stringstream ss;
                ss.imbue(std::locale::classic());
                ss << "char const " << short_symbol << "[" << sz << "]";

                f.m_head_decl = ss.str() + ";";
                f.m_char = true;
                if (md == mode::object) f.m_full_decl = ss.str();
            } else if (type == "BYTE_ARRAY") {

                get_filename();
                std::size_t sz = std::filesystem::file_size(filename.value());
                std::stringstream ss;
                ss.imbue(std::locale::classic());
                ss << "std::array<std::byte, " << sz << "> const " << short_symbol << ";";
                f.m_head_decl = ss.str();
                ss.str("");
                ss << "std::array<std::byte, " << sz << "> const " << full_symbol;
                f.m_full_decl = ss.str();
                if (md == mode::object) f.m_full_decl = ss.str();

                includes.insert("<array>");
                includes.insert("<cstddef>");
            } else if (type == "U8_ARRAY") {
                get_filename();
                std::size_t sz = std::filesystem::file_size(*filename);
                std::stringstream ss;
                ss.imbue(std::locale::classic());
                ss << "std::array<std::uint8_t, " << sz << "> " << short_symbol << ";";
                f.m_head_decl = ss.str();
                ss.str("");
                ss << "std::array<std::uint8_t, " << sz << "> " << full_symbol;

                if (md == mode::object) f.m_full_decl = ss.str();
                includes.insert("<array>");
                includes.insert("<cinttypes>");
            }

            if (md != mode::header) get_filename();


            f.m_file_path = filename;

            if (dll_export) f.m_export_dll = true;
            if (dll_import) f.m_import_dll = true;


            files.push_back(std::move(f));


        }


        std::vector<std::string> current_namespace;
        std::stringstream output;
        if (md == mode::header) {
            output << "#ifndef " << guard << std::endl;
            output << "#define " << guard << std::endl;
        }

        for (auto const &x : includes) {
            output << "#include " << x << std::endl;
        }


        for (auto const &x : files) {
            while (x.m_sym.size() < current_namespace.size()) {
                for (int i = 1; i < current_namespace.size(); i++) {
                    output << "  ";
                }
                output << "}" << std::endl;

                current_namespace.pop_back();
            }
            auto it_end = std::mismatch(current_namespace.begin(), current_namespace.end(), x.m_sym.begin());

            std::size_t dist1 = std::distance(current_namespace.begin(), it_end.first);

            while (dist1 < current_namespace.size()) {
                for (int i = 1; i < current_namespace.size(); i++) {
                    output << "  ";
                }
                output << "}" << std::endl;

                current_namespace.pop_back();
            }

            while (it_end.second != x.m_sym.end()) {
                for (int i = 0; i < current_namespace.size(); i++) {
                    output << "  ";
                }
                output << "namespace " << *it_end.second << std::endl;
                for (int i = 0; i < current_namespace.size(); i++) {
                    output << "  ";
                }
                output << "{" << std::endl;
                current_namespace.push_back(*it_end.second);

                it_end.second++;

            }

            for (int i = 0; i < current_namespace.size(); i++) {
                output << "  ";
            }
            output << "extern ";

            if (x.m_export_dll) output << "__declspec(dllexport) ";
            if (x.m_import_dll) output << "__declspec(dllimport) ";


            output << x.m_head_decl << std::endl;

        }


        while (current_namespace.size() != 0) {
            for (int i = 1; i < current_namespace.size(); i++) {
                output << "  ";
            }
            output << "}" << std::endl;

            current_namespace.pop_back();
        }


        if (md == mode::header) {
            output << "#endif" << std::endl;
        }



        if (md == mode::object)
            for (auto const &x : files) {
                output << x.m_full_decl;


                std::ifstream file(x.m_file_path.value(), std::ios::binary);

                if (!file) {
                    std::cerr << "Failed to open file: " << x.m_file_path.value() << std::endl;
                    return EXIT_FAILURE;
                }


                typename std::vector<char> vec((std::istreambuf_iterator<char>(file)),
                                               (std::istreambuf_iterator<char>()));


                if (vec.size() < 20) {
                    output << " { ";

                    for (size_t i = 0; i < vec.size(); ++i) {
                        if (x.m_char)
                            output << "char(";
                        output << "0x";

                        int a = 0;
                        int b = 0;
                        a = uint8_t(vec[i]) & 0x0F;
                        b = uint8_t(vec[i]) & 0xF0;
                        b = b >> 4;

                        char ca = 0;
                        char cb = 0;
                        if (a < 10) ca = '0' + a;
                        else ca = 'A' + a - 10;

                        if (b < 10) cb = '0' + b;
                        else cb = 'A' + b - 10;

                        output << cb << ca;

                        if (x.m_char) {
                            output << ")";
                        }
                        if (i != vec.size() - 1) output << ", ";
                        else output << " };" << std::endl;
                    }

                } else {
                    output << " {" << std::endl;
                    for (size_t i = 0; i < vec.size(); i += 20) {
                        output << "  ";
                        for (size_t z = i; z < vec.size() && z < i + 20; z++) {
                            if (x.m_char)
                                output << "char(";
                            output << "0x";

                            int a = 0;
                            int b = 0;
                            a = uint8_t(vec[z]) & 0x0F;
                            b = uint8_t(vec[z]) & 0xF0;
                            b = b >> 4;

                            char ca = 0;
                            char cb = 0;
                            if (a < 10) ca = '0' + a;
                            else ca = 'A' + a - 10;

                            if (b < 10) cb = '0' + b;
                            else cb = 'A' + b - 10;

                            output << cb << ca;

                            if (x.m_char)
                                output << ")";
                            if (z == vec.size() - 1) output << std::endl << "};" << std::endl;
                            else if (z == i + 20 - 1) output << "," << std::endl;
                            else output << ", ";
                        }
                    }
                }

            }


        std::string output_string = output.str();


        std::filesystem::path folder_path = std::filesystem::path(output_file).parent_path();

        if (!std::filesystem::exists(folder_path)) {
            std::filesystem::create_directories(folder_path);
        }

        std::ofstream out_file;
        out_file.open(output_file, std::ios::trunc);
        if (!out_file) {
            std::cerr << "Failed to open file: " << output_file << std::endl;
            return EXIT_FAILURE;
        }
        std::copy(output_string.begin(), output_string.end(), std::ostreambuf_iterator<char>(out_file));
        out_file.flush();
        out_file.close();


        return 0;
    }
    catch (int exit_code)
    {
        return exit_code;
    }
  
  
}
