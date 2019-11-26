// This software is in the public domain. Where that dedication is not
// recognized, you are granted a perpetual, irrevocable license to copy,
// distribute, and modify this file as you see fit.
// https://github.com/ddiakopoulos/tinyply
// Version 2.2

#include <thread>
#include <chrono>
#include <vector>
#include <sstream>
#include <fstream>
#include <iostream>
#include <cstring>
#include <iterator>

#include "tinyply.h"
using namespace tinyply;

#include <filesystem>
namespace fs = std::experimental::filesystem;

std::vector<std::string> iterate_directory(const fs::path & root, const std::string & extensionIncludingDot)
{
    std::vector<std::string> paths;

    for (auto & entry : fs::recursive_directory_iterator(root))
    {
        const size_t root_len = root.string().length(), ext_len = entry.path().extension().string().length();
        auto path = entry.path().string(), name = path.substr(root_len + 1, path.size() - root_len - ext_len - 1);

        for (auto & chr : name) if (chr == '\\') chr = '/';
  
        if (entry.path().extension() == extensionIncludingDot)
        {
            paths.push_back(path);
        }
    }

    return paths;
};


inline std::vector<uint8_t> read_file_binary(const std::string & pathToFile)
{
    std::ifstream file(pathToFile, std::ios::binary);
    std::vector<uint8_t> fileBufferBytes;

    if (file.is_open())
    {
        file.seekg(0, std::ios::end);
        size_t sizeBytes = file.tellg();
        file.seekg(0, std::ios::beg);
        fileBufferBytes.resize(sizeBytes);
        if (file.read((char*)fileBufferBytes.data(), sizeBytes)) return fileBufferBytes;
    }
    else throw std::runtime_error("could not open binary ifstream to path " + pathToFile);
    return fileBufferBytes;
}


struct membuf : std::streambuf 
{
    membuf(char const* base, size_t size) 
    {
        char* p(const_cast<char*>(base));
        this->setg(p, p, p + size);
    }
};

struct imemstream : virtual membuf, std::istream 
{
    imemstream(char const* base, size_t size) : membuf(base, size), std::istream(static_cast<std::streambuf*>(this)) {}
};

class manual_timer
{
    std::chrono::high_resolution_clock::time_point t0;
    double timestamp{ 0.f };
public:
    void start() { t0 = std::chrono::high_resolution_clock::now(); }
    void stop() { timestamp = std::chrono::duration<float>(std::chrono::high_resolution_clock::now() - t0).count() * 1000; }
    const double & get() { return timestamp; }
};

struct float2 { float x, y; };
struct float3 { float x, y, z; };
struct double3 { double x, y, z; };
struct uint3 { uint32_t x, y, z; };
struct uint4 { uint32_t x, y, z, w; };

struct geometry
{
    std::vector<float3> vertices;
    std::vector<float3> normals;
    std::vector<float2> texcoords;
    std::vector<uint3> triangles;
};

inline geometry make_cube_geometry()
{
    geometry cube;

    const struct CubeVertex { float3 position, normal; float2 texCoord; } verts[] = {
    { { -1, -1, -1 },{ -1, 0, 0 },{ 0, 0 } },{ { -1, -1, +1 },{ -1, 0, 0 },{ 1, 0 } },{ { -1, +1, +1 },{ -1, 0, 0 },{ 1, 1 } },{ { -1, +1, -1 },{ -1, 0, 0 },{ 0, 1 } },
    { { +1, -1, +1 },{ +1, 0, 0 },{ 0, 0 } },{ { +1, -1, -1 },{ +1, 0, 0 },{ 1, 0 } },{ { +1, +1, -1 },{ +1, 0, 0 },{ 1, 1 } },{ { +1, +1, +1 },{ +1, 0, 0 },{ 0, 1 } },
    { { -1, -1, -1 },{ 0, -1, 0 },{ 0, 0 } },{ { +1, -1, -1 },{ 0, -1, 0 },{ 1, 0 } },{ { +1, -1, +1 },{ 0, -1, 0 },{ 1, 1 } },{ { -1, -1, +1 },{ 0, -1, 0 },{ 0, 1 } },
    { { +1, +1, -1 },{ 0, +1, 0 },{ 0, 0 } },{ { -1, +1, -1 },{ 0, +1, 0 },{ 1, 0 } },{ { -1, +1, +1 },{ 0, +1, 0 },{ 1, 1 } },{ { +1, +1, +1 },{ 0, +1, 0 },{ 0, 1 } },
    { { -1, -1, -1 },{ 0, 0, -1 },{ 0, 0 } },{ { -1, +1, -1 },{ 0, 0, -1 },{ 1, 0 } },{ { +1, +1, -1 },{ 0, 0, -1 },{ 1, 1 } },{ { +1, -1, -1 },{ 0, 0, -1 },{ 0, 1 } },
    { { -1, +1, +1 },{ 0, 0, +1 },{ 0, 0 } },{ { -1, -1, +1 },{ 0, 0, +1 },{ 1, 0 } },{ { +1, -1, +1 },{ 0, 0, +1 },{ 1, 1 } },{ { +1, +1, +1 },{ 0, 0, +1 },{ 0, 1 } }};

    std::vector<uint4> quads = { { 0, 1, 2, 3 },{ 4, 5, 6, 7 },{ 8, 9, 10, 11 },{ 12, 13, 14, 15 },{ 16, 17, 18, 19 },{ 20, 21, 22, 23 } };

    for (auto & q : quads)
    {
        cube.triangles.push_back({ q.x,q.y,q.z });
        cube.triangles.push_back({ q.x,q.z,q.w });
    }

    for (int i = 0; i < 24; ++i)
    {
        cube.vertices.push_back(verts[i].position);
        cube.normals.push_back(verts[i].normal);
        cube.texcoords.push_back(verts[i].texCoord);
    }

    return cube;
}

void write_ply_example(const std::string & filename)
{
    geometry cube = make_cube_geometry();

    std::filebuf fb_binary;
	fb_binary.open(filename + "-binary.ply", std::ios::out | std::ios::binary);
    std::ostream outstream_binary(&fb_binary);
	if (outstream_binary.fail()) throw std::runtime_error("failed to open " + filename);

	std::filebuf fb_ascii;
	fb_ascii.open(filename + "-ascii.ply", std::ios::out);
	std::ostream outstream_ascii(&fb_ascii);
	if (outstream_ascii.fail()) throw std::runtime_error("failed to open " + filename);

    PlyFile cube_file;

    cube_file.add_properties_to_element("vertex", { "x", "y", "z" }, 
        Type::FLOAT32, cube.vertices.size(), reinterpret_cast<uint8_t*>(cube.vertices.data()), Type::INVALID, 0);

    cube_file.add_properties_to_element("vertex", { "nx", "ny", "nz" },
        Type::FLOAT32, cube.normals.size(), reinterpret_cast<uint8_t*>(cube.normals.data()), Type::INVALID, 0);

    cube_file.add_properties_to_element("vertex", { "u", "v" },
        Type::FLOAT32, cube.texcoords.size() , reinterpret_cast<uint8_t*>(cube.texcoords.data()), Type::INVALID, 0);

    cube_file.add_properties_to_element("face", { "vertex_indices" },
        Type::UINT32, cube.triangles.size(), reinterpret_cast<uint8_t*>(cube.triangles.data()), Type::UINT8, 3);

    cube_file.get_comments().push_back("generated by tinyply 2.2");

	// Write an ASCII file
    cube_file.write(outstream_ascii, false);

	// Write a binary file
	cube_file.write(outstream_binary, true);
}

float read_ply_file(const std::string & filepath)
{
	try
	{
        auto bytes = read_file_binary(filepath);
        imemstream ss((char*)bytes.data(), bytes.size());

		//std::ifstream ss(filepath, std::ios::binary);

		if (ss.fail()) throw std::runtime_error("failed to open " + filepath);

		PlyFile file;
		file.parse_header(ss);

		//std::cout << "........................................................................\n";
		//for (auto c : file.get_comments()) std::cout << "Comment: " << c << std::endl;
		//for (auto e : file.get_elements())
		//{
		//	std::cout << "element - " << e.name << " (" << e.size << ")" << std::endl;
		//	for (auto p : e.properties) std::cout << "\tproperty - " << p.name << " (" << tinyply::PropertyTable[p.propertyType].str << ")" << std::endl;
		//}
		//std::cout << "........................................................................\n";

		// Tinyply treats parsed data as untyped byte buffers. See below for examples.
		std::shared_ptr<PlyData> vertices, normals, faces, texcoords;

		// The header information can be used to programmatically extract properties on elements
		// known to exist in the header prior to reading the data. For brevity of this sample, properties 
		// like vertex position are hard-coded: 
		try { vertices = file.request_properties_from_element("vertex", { "x", "y", "z" }); }
		catch (const std::exception & e) { std::cerr << "tinyply exception: " << e.what() << std::endl; }

		//try { normals = file.request_properties_from_element("vertex", { "nx", "ny", "nz" }); }
		//catch (const std::exception & e) { std::cerr << "tinyply exception: " << e.what() << std::endl; }

		//try { texcoords = file.request_properties_from_element("vertex", { "u", "v" }); }
		//catch (const std::exception & e) { std::cerr << "tinyply exception: " << e.what() << std::endl; }

		// Providing a list size hint (the last argument) is a 2x performance improvement. If you have 
		// arbitrary ply files, it is best to leave this 0. 
		try { faces = file.request_properties_from_element("face", { "vertex_indices" }, 0); }
		catch (const std::exception & e) { std::cerr << "tinyply exception: " << e.what() << std::endl; }

		manual_timer read_timer;

		read_timer.start();
		file.read(ss);
		read_timer.stop();

		// std::cout << "Reading took " << read_timer.get() / 1000.f << " seconds." << std::endl;
		// if (vertices) std::cout << "\tRead " << vertices->count << " total vertices "<< std::endl;
		// if (normals) std::cout << "\tRead " << normals->count << " total vertex normals " << std::endl;
		// if (texcoords) std::cout << "\tRead " << texcoords->count << " total vertex texcoords " << std::endl;
		// if (faces) std::cout << "\tRead " << faces->count << " total faces (triangles) " << std::endl;

		// type casting to your own native types - Option A
		// {
		// 	const size_t numVerticesBytes = vertices->buffer.size_bytes();
		// 	std::vector<float3> verts(vertices->count);
		// 	std::memcpy(verts.data(), vertices->buffer.get(), numVerticesBytes);
		// }
        // 
		// // type casting to your own native types - Option B
		// {
		// 	std::vector<float3> verts_floats;
		// 	std::vector<double3> verts_doubles;
		// 	if (vertices->t == tinyply::Type::FLOAT32) { /* as floats ... */ }
		// 	if (vertices->t == tinyply::Type::FLOAT64) { /* as doubles ... */ }
		// }

        return read_timer.get() / 1000.f;
	}
	catch (const std::exception & e)
	{
		std::cerr << "Caught tinyply exception: " << e.what() << std::endl;
	}

    return 0.f;
}

int main(int argc, char *argv[])
{
	//write_ply_example("example_cube");
	//read_ply_file("lucy.ply");
	//read_ply_file("example_cube-binary.ply");

    float sum_timer = 0.f;

    auto files = iterate_directory("../assets/pbrt/", ".ply");

    for (auto & path : files)
    {
        sum_timer += read_ply_file(path);
    }

    std::cout << "timer! " << sum_timer << std::endl;
     return EXIT_SUCCESS;
}
