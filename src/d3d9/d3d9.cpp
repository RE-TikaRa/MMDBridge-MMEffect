
#define CINTERFACE

#include "d3d9.h"
#include "d3dx9.h"
#include <windows.h>
#include <vector>
#include <string>
#include <sstream>
#include <tchar.h>
#include <fstream>
#include <algorithm>
#include <shlwapi.h>

#include <pybind11/eval.h>
#include <pybind11/stl_bind.h>
namespace py = pybind11;

#include <commctrl.h>
#include <richedit.h>

#include <process.h>

#include "bridge_parameter.h"
#include "alembic.h"
#include "vmd.h"
#include "pmx.h"
#include "resource.h"
#include "MMDExport.h"
#include "UMStringUtil.h"
#include "UMPath.h"

#ifdef _WIN64
#define _LONG_PTR LONG_PTR
#else
#define _LONG_PTR LONG
#endif

template <class T> std::string to_string(T value)
{
	return umbase::UMStringUtil::number_to_string(value);
}

//ワイド文字列からutf8文字列に変換
static void to_string(std::string &dest, const std::wstring &src) 
{
	dest = umbase::UMStringUtil::wstring_to_utf8(src);
}

static void messagebox(std::string title, std::string message)
{
	::MessageBoxA(NULL, message.c_str(), title.c_str(), MB_OK);
}

static void message(std::string message)
{
	::MessageBoxA(NULL, message.c_str(), "message", MB_OK);
}

static void messagebox_float4(float v[4], const char *title)
{
	::MessageBoxA(NULL, std::string(
		to_string(v[0]) + " "
		+ to_string(v[1]) + " "
		+ to_string(v[2]) + " "
		+ to_string(v[3]) + "\n").c_str(), title, MB_OK);
}

static void messagebox_matrix(D3DXMATRIX& mat, const char *title)
{
	::MessageBoxA(NULL,
		std::string(
		to_string(mat._11)+" "+to_string(mat._12)+" "+to_string(mat._13)+" "+to_string(mat._14)+"\n"
		+to_string(mat._21)+" "+to_string(mat._22)+" "+to_string(mat._23)+" "+to_string(mat._24)+"\n"
		+to_string(mat._31)+" "+to_string(mat._32)+" "+to_string(mat._33)+" "+to_string(mat._34)+"\n"
		+to_string(mat._41)+" "+to_string(mat._42)+" "+to_string(mat._43)+" "+to_string(mat._44)+"\n").c_str(), title, MB_OK);
}

// IDirect3DDevice9のフック関数
void hookDevice(void);
void originalDevice(void);
// フックしたデバイス
IDirect3DDevice9 *p_device = NULL;

RenderData renderData;

std::vector<std::pair<IDirect3DTexture9*, bool> > finishTextureBuffers;

std::map<IDirect3DTexture9*, RenderedTexture> renderedTextures;
std::map<int, std::map<int , RenderedMaterial*> > renderedMaterials;
//-----------------------------------------------------------------------------------------------------------------

static bool writeTextureToFile(const std::string &texturePath, IDirect3DTexture9 * texture, D3DXIMAGE_FILEFORMAT fileFormat);

static bool writeTextureToFiles(const std::string &texturePath, const std::string &textureType, bool uncopied = false);

static bool copyTextureToFiles(const std::u16string &texturePath);

static bool writeTextureToMemory(const std::string &textureName, IDirect3DTexture9 * texture, bool copied);

//------------------------------------------Python呼び出し--------------------------------------------------------
static int pre_frame = 0;
static int presentCount = 0;
static int process_frame = -1;
static int ui_frame = 0;

// 行列で3Dベクトルをトランスフォームする
// D3DXVec3Transformとほぼ同じ
static void d3d_vector3_dir_transform(
	D3DXVECTOR3 &dst, 
	const D3DXVECTOR3 &src, 
	const D3DXMATRIX &matrix)
{
	const float tmp[] = {
		src.x*matrix.m[0][0] + src.y*matrix.m[1][0] + src.z*matrix.m[2][0],
		src.x*matrix.m[0][1] + src.y*matrix.m[1][1] + src.z*matrix.m[2][1],
		src.x*matrix.m[0][2] + src.y*matrix.m[1][2] + src.z*matrix.m[2][2]
	};
	dst.x = tmp[0];
	dst.y = tmp[1];
	dst.z = tmp[2];
}

static void d3d_vector3_transform(
	D3DXVECTOR3 &dst, 
	const D3DXVECTOR3 &src, 
	const D3DXMATRIX &matrix)
{
	const float tmp[] = {
		src.x*matrix.m[0][0] + src.y*matrix.m[1][0] + src.z*matrix.m[2][0] + 1.0f*matrix.m[3][0],
		src.x*matrix.m[0][1] + src.y*matrix.m[1][1] + src.z*matrix.m[2][1] + 1.0f*matrix.m[3][1],
		src.x*matrix.m[0][2] + src.y*matrix.m[1][2] + src.z*matrix.m[2][2] + 1.0f*matrix.m[3][2]
	};
	dst.x = tmp[0];
	dst.y = tmp[1];
	dst.z = tmp[2];
}

// python
namespace
{
	std::wstring pythonName; // スクリプト名
	int script_call_setting = 2; // スクリプト呼び出し設定
	std::map<int, int> exportedFrames;
	enum class UiLanguage
	{
		Chinese,
		Source
	};

	struct UiText
	{
		const wchar_t* dialog_title;
		const wchar_t* ok;
		const wchar_t* cancel;
		const wchar_t* script_label;
		const wchar_t* call_setting_label;
		const wchar_t* rescan;
		const wchar_t* frame_range_label;
		const wchar_t* frame_separator;
		const wchar_t* fps_hint;
		const wchar_t* fps;
		const wchar_t* call_setting_options[2];
		const wchar_t* menu_setting;
		const wchar_t* menu_language;
		const wchar_t* menu_language_chinese;
		const wchar_t* menu_language_source;
		const wchar_t* reset_message;
		const wchar_t* reset_caption;
	};

	UiLanguage ui_language = UiLanguage::Chinese;

	const UiText& current_ui_text()
	{
		static const UiText chinese = {
			L"MMDBridge - \u63d2\u4ef6\u8bbe\u7f6e",
			L"\u786e\u5b9a",
			L"\u53d6\u6d88",
			L"\u4f7f\u7528\u811a\u672c",
			L"\u811a\u672c\u8c03\u7528\u8bbe\u7f6e",
			L"\u91cd\u65b0\u626b\u63cf",
			L"\u8f93\u51fa\u76ee\u6807\u5e27",
			L"\uff5e",
			L"\u5e27\u7387\uff08\u8bf7\u4e0e AVI \u8f93\u51fa\u8bbe\u7f6e\u4fdd\u6301\u4e00\u81f4\uff09",
			L"fps",
			{ L"\u6267\u884c", L"\u4e0d\u6267\u884c" },
			L"\u63d2\u4ef6\u8bbe\u7f6e",
			L"\u8bed\u8a00",
			L"\u4e2d\u6587",
			L"\u539f\u6587\uff08\u65e5\u8bed\uff09",
			L"MMDBridge \u6682\u4e0d\u652f\u6301 3D Vision",
			L"MMDBridge"
		};
		static const UiText source = {
			L"MMDBridge - \u30d7\u30e9\u30b0\u30a4\u30f3\u8a2d\u5b9a",
			L"OK",
			L"\u30ad\u30e3\u30f3\u30bb\u30eb",
			L"\u4f7f\u7528\u3059\u308b\u30b9\u30af\u30ea\u30d7\u30c8",
			L"\u30b9\u30af\u30ea\u30d7\u30c8\u306e\u547c\u3073\u51fa\u3057\u8a2d\u5b9a",
			L"\u518d\u691c\u7d22",
			L"\u51fa\u529b\u5bfe\u8c61\u30d5\u30ec\u30fc\u30e0",
			L"\uff5e",
			L"\u30d5\u30ec\u30fc\u30e0\u30ec\u30fc\u30c8(AVI\u51fa\u529b\u8a2d\u5b9a\u3068\u304a\u306a\u3058\u306b\u3057\u3066\u304f\u3060\u3055\u3044)",
			L"fps",
			{ L"\u5b9f\u884c\u3059\u308b", L"\u5b9f\u884c\u3057\u306a\u3044" },
			L"\u30d7\u30e9\u30b0\u30a4\u30f3\u8a2d\u5b9a",
			L"\u8a00\u8a9e",
			L"\u4e2d\u56fd\u8a9e",
			L"\u539f\u6587\uff08\u65e5\u672c\u8a9e\uff09",
			L"MMDBridge\u306f\u30013D vision \u672a\u5bfe\u5fdc\u3067\u3059",
			L"MMDBridge"
		};
		return ui_language == UiLanguage::Chinese ? chinese : source;
	}

	/// スクリプトのリロード.
	bool relaod_python_script()
	{
		BridgeParameter::mutable_instance().mmdbridge_python_script.clear();
		std::ifstream ifs(BridgeParameter::instance().python_script_path.c_str());
		if (!ifs) return false;
		char buf[2048];
		while (ifs.getline( buf, sizeof(buf))) {
			BridgeParameter::mutable_instance().mmdbridge_python_script.append(buf);
			BridgeParameter::mutable_instance().mmdbridge_python_script.append("\r\n");
		}
		ifs.close();
		return true;
	}

	/// スクリプトパスのリロード.
	void reload_python_file_paths()
	{
		BridgeParameter& mutable_parameter = BridgeParameter::mutable_instance();
		std::wstring searchPath = mutable_parameter.base_path;
		std::wstring searchStr(searchPath + _T("*.py"));
		const std::wstring selected_name = mutable_parameter.python_script_name;
		const std::wstring selected_path = mutable_parameter.python_script_path;
		bool has_selected_script = false;

		mutable_parameter.python_script_name_list.clear();
		mutable_parameter.python_script_path_list.clear();

		// pythonファイル検索
		WIN32_FIND_DATA find;
		HANDLE hFind = FindFirstFile(searchStr.c_str(), &find);
		if (hFind != INVALID_HANDLE_VALUE)
		{
			do
			{
				if(! (find.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
				{
					std::wstring name( find.cFileName);
					std::wstring path(searchPath + find.cFileName);
					if (name == selected_name && path == selected_path)
					{
						has_selected_script = true;
					}
					mutable_parameter.python_script_name_list.push_back(name);
					mutable_parameter.python_script_path_list.push_back(path);
				}
			} while(FindNextFile(hFind, &find));
			FindClose(hFind);
		}

		if (mutable_parameter.python_script_name_list.empty())
		{
			mutable_parameter.python_script_name.clear();
			mutable_parameter.python_script_path.clear();
		}
		else if (!has_selected_script)
		{
			mutable_parameter.python_script_name = mutable_parameter.python_script_name_list.front();
			mutable_parameter.python_script_path = mutable_parameter.python_script_path_list.front();
		}
	}

	// Get a reference to the main module.
	PyObject* main_module = NULL; 

	// Get the main module's dictionary
	// and make a copy of it.
	PyObject* main_dict = NULL;

	int get_vertex_buffer_size()
	{
		return BridgeParameter::instance().finish_buffer_list.size();
	}

	int get_vertex_size(int at)
	{
		return BridgeParameter::instance().render_buffer(at).vertecies.size();
	}

	std::vector<float> get_vertex(int at, int vpos)
	{
		const RenderedBuffer& buffer = BridgeParameter::instance().render_buffer(at);
		float x = buffer.vertecies[vpos].x;
		float y = buffer.vertecies[vpos].y;
		float z = buffer.vertecies[vpos].z;
		std::vector<float> result;
		result.push_back(x);
		result.push_back(y);
		result.push_back(z);
		return result;
	}

	int get_normal_size(int at)
	{
		return BridgeParameter::instance().render_buffer(at).normals.size();
	}

	std::vector<float> get_normal(int at, int vpos)
	{
		const RenderedBuffer& buffer = BridgeParameter::instance().render_buffer(at);
		float x = buffer.normals[vpos].x;
		float y = buffer.normals[vpos].y;
		float z = buffer.normals[vpos].z;
		std::vector<float> result;
		result.push_back(x);
		result.push_back(y);
		result.push_back(z);
		return result;
	}

	int get_uv_size(int at)
	{
		return BridgeParameter::instance().render_buffer(at).uvs.size();
	}

	std::vector<float> get_uv(int at, int vpos)
	{
		const RenderedBuffer& buffer = BridgeParameter::instance().render_buffer(at);
		float u = buffer.uvs[vpos].x;
		float v = buffer.uvs[vpos].y;
		std::vector<float> result;
		result.push_back(u);
		result.push_back(v);
		return result;
	}

	int get_material_size(int at)
	{
		return BridgeParameter::instance().render_buffer(at).materials.size();
	}

	bool is_accessory(int at)
	{
		int result = 0;
		if (BridgeParameter::instance().render_buffer(at).isAccessory)
		{
			return true;
		}
		return false;
	}

	int get_pre_accessory_count()
	{
		return ExpGetPreAcsNum();
	}

	std::vector<float> get_diffuse(int at, int mpos)
	{
		RenderedMaterial* mat = BridgeParameter::instance().render_buffer(at).materials[mpos];
		std::vector<float> result;
		result.push_back(mat->diffuse.x);
		result.push_back(mat->diffuse.y);
		result.push_back(mat->diffuse.z);
		result.push_back(mat->diffuse.w);
		return result;
	}

	std::vector<float> get_ambient(int at, int mpos)
	{
		RenderedMaterial* mat = BridgeParameter::instance().render_buffer(at).materials[mpos];
		std::vector<float> result;
		result.push_back(mat->ambient.x);
		result.push_back(mat->ambient.y);
		result.push_back(mat->ambient.z);
		return result;
	}

	std::vector<float> get_specular(int at, int mpos)
	{
		RenderedMaterial* mat = BridgeParameter::instance().render_buffer(at).materials[mpos];
		std::vector<float> result;
		result.push_back(mat->specular.x);
		result.push_back(mat->specular.y);
		result.push_back(mat->specular.z);
		return result;
	}

	std::vector<float> get_emissive(int at, int mpos)
	{
		RenderedMaterial* mat = BridgeParameter::instance().render_buffer(at).materials[mpos];
		std::vector<float> result;
		result.push_back(mat->emissive.x);
		result.push_back(mat->emissive.y);
		result.push_back(mat->emissive.z);
		return result;
	}

	float get_power(int at, int mpos)
	{
		RenderedMaterial* mat = BridgeParameter::instance().render_buffer(at).materials[mpos];
		float power = mat->power;
		return power;
	}

	std::string get_texture(int at, int mpos)
	{
		RenderedMaterial* mat = BridgeParameter::instance().render_buffer(at).materials[mpos];
		return mat->texture;
	}

	std::string get_exported_texture(int at, int mpos)
	{
		RenderedMaterial* mat = BridgeParameter::instance().render_buffer(at).materials[mpos];
		return mat->memoryTexture;
	}

	int get_face_size(int at, int mpos)
	{
		return BridgeParameter::instance().render_buffer(at).materials[mpos]->surface.faces.size();
	}

	std::vector<int> get_face(int at, int mpos, int fpos)
	{
		RenderedSurface &surface = BridgeParameter::instance().render_buffer(at).materials[mpos]->surface;
		int v1 = surface.faces[fpos].x;
		int v2 = surface.faces[fpos].y;
		int v3 = surface.faces[fpos].z;
		std::vector<int> result;
		result.push_back(v1);
		result.push_back(v2);
		result.push_back(v3);
		return result;
	}

	int get_texture_buffer_size()
	{
		return finishTextureBuffers.size();
	}

	std::vector<float> get_texture_size(int at)
	{
		std::vector<float> result;
		result.push_back(renderedTextures[finishTextureBuffers[at].first].size.x);
		result.push_back(renderedTextures[finishTextureBuffers[at].first].size.y);
		return result;
	}

	std::string get_texture_name(int at)
	{
		return renderedTextures[finishTextureBuffers[at].first].name;
	}

	std::vector<float> get_texture_pixel(int at, int tpos)
	{
		UMVec4f &rgba = renderedTextures[finishTextureBuffers[at].first].texture[tpos];
		std::vector<float> result;
		result.push_back(rgba.x);
		result.push_back(rgba.y);
		result.push_back(rgba.z);
		result.push_back(rgba.w);
		return result;
	}

	bool export_texture(int at, int mpos, const std::string& dst)
	{
		RenderedMaterial* mat = BridgeParameter::instance().render_buffer(at).materials[mpos];
		std::string path(dst);
		std::string textureType = path.substr(path.size() - 3, 3);

		D3DXIMAGE_FILEFORMAT fileFormat;
		if (textureType == "bmp" || textureType == "BMP") { fileFormat = D3DXIFF_BMP; }
		else if (textureType == "png" || textureType == "PNG") { fileFormat = D3DXIFF_PNG; }
		else if (textureType == "jpg" || textureType == "JPG") { fileFormat = D3DXIFF_JPG; }
		else if (textureType == "tga" || textureType == "TGA") { fileFormat = D3DXIFF_TGA; }
		else if (textureType == "dds" || textureType == "DDS") { fileFormat = D3DXIFF_DDS; }
		else if (textureType == "ppm" || textureType == "PPM") { fileFormat = D3DXIFF_PPM; }
		else if (textureType == "dib" || textureType == "DIB") { fileFormat = D3DXIFF_DIB; }
		else if (textureType == "hdr" || textureType == "HDR") { fileFormat = D3DXIFF_HDR; }
		else if (textureType == "pfm" || textureType == "PFM") { fileFormat = D3DXIFF_PFM; }
		else { return false; }

		if (mat->tex)
		{
			return writeTextureToFile(path, mat->tex, fileFormat);
		}
		return false;
	}

	bool export_textures(const std::string& p, const std::string& t)
	{
		std::u16string path = umbase::UMStringUtil::utf8_to_utf16(p);
		std::string type(t);
		if (umbase::UMPath::exists(path))
		{
			return writeTextureToFiles(p, t);
		}
		return false;
	}

	bool export_uncopied_textures(const std::string& p, const std::string& t)
	{
		std::u16string path = umbase::UMStringUtil::utf8_to_utf16(p);
		if (umbase::UMPath::exists(path))
		{
			return writeTextureToFiles(p, t, true);
		}
		return false;
	}

	bool copy_textures(const std::string& s)
	{
		std::u16string path = umbase::UMStringUtil::utf8_to_utf16(s);
		if (umbase::UMPath::exists(path))
		{
			std::wstring wpath = umbase::UMStringUtil::utf16_to_wstring(path);
			return copyTextureToFiles(path);
		}
		return false;
	}

	std::string get_base_path()
	{
		std::string path = umbase::UMStringUtil::wstring_to_utf8(BridgeParameter::instance().base_path);
		return path;
	}

	std::vector<float> get_camera_up()
	{
		D3DXVECTOR3 v;
		D3DXVECTOR3 dst;
		UMGetCameraUp(&v);
		d3d_vector3_dir_transform(dst, v, BridgeParameter::instance().first_noaccessory_buffer().world_inv);
		std::vector<float> result;
		result.push_back(dst.x);
		result.push_back(dst.y);
		result.push_back(dst.z);
		return result;
	}

	std::vector<float> get_camera_up_org()
	{
		D3DXVECTOR3 v;
		UMGetCameraUp(&v);
		std::vector<float> result;
		result.push_back(v.x);
		result.push_back(v.y);
		result.push_back(v.z);
		return result;
	}
	
	std::vector<float> get_camera_at()
	{
		D3DXVECTOR3 v;
		D3DXVECTOR3 dst;
		UMGetCameraAt(&v);
		d3d_vector3_transform(dst, v, BridgeParameter::instance().first_noaccessory_buffer().world_inv);
		std::vector<float> result;
		result.push_back(dst.x);
		result.push_back(dst.y);
		result.push_back(dst.z);
		return result;
	}

	std::vector<float> get_camera_eye()
	{
		D3DXVECTOR3 v;
		D3DXVECTOR3 dst;
		UMGetCameraEye(&v);
		d3d_vector3_transform(dst, v, BridgeParameter::instance().first_noaccessory_buffer().world_inv);
		std::vector<float> result;
		result.push_back(dst.x);
		result.push_back(dst.y);
		result.push_back(dst.z);
		return result;
	}

	std::vector<float> get_camera_eye_org()
	{
		D3DXVECTOR3 v;
		UMGetCameraEye(&v);
		std::vector<float> result;
		result.push_back(v.x);
		result.push_back(v.y);
		result.push_back(v.z);
		return result;
	}

	float get_camera_fovy()
	{
		D3DXVECTOR4 v;
		UMGetCameraFovLH(&v);
		return  v.x;
	}

	float get_camera_aspect()
	{
		D3DXVECTOR4 v;
		UMGetCameraFovLH(&v);
		return v.y;
	}

	float get_camera_near()
	{
		D3DXVECTOR4 v;
		UMGetCameraFovLH(&v);
		return v.z;
	}

	float get_camera_far()
	{
		D3DXVECTOR4 v;
		UMGetCameraFovLH(&v);
		return v.w;
	}
	
	int get_frame_number()
	{
		if (process_frame >= 0) 
		{
			return process_frame;
		}
		else
		{
			return ui_frame;
		}
	}

	int get_start_frame()
	{
		return BridgeParameter::instance().start_frame;
	}
	
	int get_end_frame()
	{
		return BridgeParameter::instance().end_frame;
	}

	int get_frame_width()
	{
		return BridgeParameter::instance().frame_width;
	}

	int get_frame_height()
	{
		return BridgeParameter::instance().frame_height;
	}

	int get_export_fps()
	{
		return BridgeParameter::instance().export_fps;
	}

	std::vector<float> get_light(int at)
	{
		const UMVec3f &light = BridgeParameter::instance().render_buffer(at).light;
		std::vector<float> result;
		result.push_back(light.x);
		result.push_back(light.y);
		result.push_back(light.z);
		return result;
	}

	std::vector<float> get_light_color(int at)
	{
		const UMVec3f &light = BridgeParameter::instance().render_buffer(at).light_color;
		std::vector<float> result;
		result.push_back(light.x);
		result.push_back(light.y);
		result.push_back(light.z);
		return result;
	}

	int get_object_size()
	{
		return ExpGetPmdNum();
	}

	int get_bone_size(int at)
	{
		return ExpGetPmdBoneNum(at);
	}


	int get_accessory_size()
	{
		return ExpGetAcsNum();
	}

	std::string get_accessory_filename(int at)
	{
		const char* sjis = ExpGetAcsFilename(at);
		const int size = ::MultiByteToWideChar(CP_ACP, 0, (LPCSTR)sjis, -1, NULL, 0);
		wchar_t* utf16 = new wchar_t[size];
		::MultiByteToWideChar(CP_ACP, 0, (LPCSTR)sjis, -1, (LPWSTR)utf16, size);
		std::wstring wchar(utf16);
		delete[] utf16;
		std::string utf8str = umbase::UMStringUtil::wstring_to_utf8(wchar);
		return utf8str;
	}

	std::string get_object_filename(int at)
	{
		const int count = get_bone_size(at);
		if (count <= 0) return "";
		const char* sjis = ExpGetPmdFilename(at);
		const int size = ::MultiByteToWideChar(CP_ACP, 0, (LPCSTR)sjis, -1, NULL, 0);
		wchar_t* utf16 = new wchar_t[size];
		::MultiByteToWideChar(CP_ACP, 0, (LPCSTR)sjis, -1, (LPWSTR)utf16, size);
		std::wstring wchar(utf16);
		delete [] utf16;
		std::string utf8str = umbase::UMStringUtil::wstring_to_utf8(wchar);
		return utf8str;
	}

	std::string get_buffer_filename(int at)
	{
		auto& buffer = BridgeParameter::instance().render_buffer(at);
		if (buffer.isAccessory)
		{
			return get_accessory_filename(buffer.order);
		}
		else
		{
			return get_object_filename(buffer.order);
		}
	}

	std::string get_bone_name(int at, int bone_index)
	{
		const int count = get_bone_size(at);
		if (count <= 0) return "";
		const char* sjis = ExpGetPmdBoneName(at, bone_index);
		const int size = ::MultiByteToWideChar(CP_ACP, 0, (LPCSTR)sjis, -1, NULL, 0);
		wchar_t* utf16 = new wchar_t[size];
		::MultiByteToWideChar(CP_ACP, 0, (LPCSTR)sjis, -1, (LPWSTR)utf16, size);
		std::wstring wchar(utf16);
		delete [] utf16;
		std::string utf8str = umbase::UMStringUtil::wstring_to_utf8(wchar);
		return utf8str;
	}

	std::vector<float> get_bone_matrix(int at, int bone_index)
	{
		const int count = get_bone_size(at);
		std::vector<float> result;
		if (count <= 0) return result;

		D3DMATRIX mat = ExpGetPmdBoneWorldMat(at, bone_index);
		for (int i = 0; i < 4; ++i)
		{
			for (int k = 0; k < 4; ++k)
			{
				result.push_back(mat.m[i][k]);
			}
		}
		return result;
	}

	std::vector<float> get_world(int at)
	{
		const D3DXMATRIX& world = BridgeParameter::instance().render_buffer(at).world;
		std::vector<float> result;
		for (int i = 0; i < 4; ++i)
		{
			for (int k = 0; k < 4; ++k)
			{
				result.push_back(world.m[i][k]);
			}
		}
		return result;
	}

	std::vector<float> get_world_inv(int at)
	{
		const D3DXMATRIX& world_inv = BridgeParameter::instance().render_buffer(at).world_inv;
		std::vector<float> result;
		for (int i = 0; i < 4; ++i)
		{
			for (int k = 0; k < 4; ++k)
			{
				result.push_back(world_inv.m[i][k]);
			}
		}
		return result;
	}

	std::vector<float> get_view(int at)
	{
		const D3DXMATRIX& view = BridgeParameter::instance().render_buffer(at).view;
		std::vector<float> result;
		for (int i = 0; i < 4; ++i)
		{
			for (int k = 0; k < 4; ++k)
			{
				result.push_back(view.m[i][k]);
			}
		}
		return result;
	}

	std::vector<float> get_projection(int at)
	{
		const D3DXMATRIX& projection = BridgeParameter::instance().render_buffer(at).projection;
		std::vector<float> result;
		for (int i = 0; i < 4; ++i)
		{
			for (int k = 0; k < 4; ++k)
			{
				result.push_back(projection.m[i][k]);
			}
		}
		return result;
	}

	std::vector<float> invert_matrix(const std::vector<float> &tp1)
	{
		if (tp1.size() < 16) {
			PyErr_SetString(PyExc_IndexError, "index out of range");
			throw py::error_already_set();
		}
		std::vector<float> result;
		UMMat44d src;
		for (int i = 0; i < 4; ++i) {
			for (int k = 0; k < 4; ++k) {
				src[i][k] = static_cast<double>(tp1[i * 4 + k]);
			}
		}
		const UMMat44d dst = src.inverted();
		for (int i = 0; i < 4; ++i) {
			for (int k = 0; k < 4; ++k) {
				result.push_back(dst[i][k]);
			}
		}
		return result;
	}

	std::vector<float> extract_xyz_degree(const std::vector<float> &tp1)
	{
		if (tp1.size() < 16) {
			PyErr_SetString(PyExc_IndexError, "index out of range");
			throw py::error_already_set();
		}
		std::vector<float> result;
		UMMat44d src;
		for (int i = 0; i < 4; ++i) {
			for (int k = 0; k < 4; ++k) {
				src[i][k] = static_cast<double>(tp1[i * 4 + k]);
			}
		}

		const UMVec3d euler = umbase::um_matrix_to_euler_xyz(src);
		for (int i = 0; i < 3; ++i) {
			result.push_back(umbase::um_to_degree(euler[i]));
		}
		return result;
	}

	bool set_texture_buffer_enabled(bool enabled)
	{
		BridgeParameter::mutable_instance().is_texture_buffer_enabled = enabled;
		return true;
	}

	bool set_int_value(int pos, int value)
	{
		BridgeParameter::mutable_instance().py_int_map[pos] = value;
		return true;
	}

	bool set_float_value(int pos, float value)
	{
		BridgeParameter::mutable_instance().py_float_map[pos] = value;
		return true;
	}

	int get_int_value(int pos)
	{
		if (BridgeParameter::instance().py_int_map.find(pos) != BridgeParameter::instance().py_int_map.end())
		{
			return BridgeParameter::mutable_instance().py_int_map[pos];
		}
		return 0;
	}

	float get_float_value(int pos)
	{
		if (BridgeParameter::instance().py_float_map.find(pos) != BridgeParameter::instance().py_float_map.end())
		{
			return BridgeParameter::mutable_instance().py_float_map[pos];
		}
		return 0;
	}

	std::vector<float> d3dx_vec3_normalize(float x, float y, float z)
	{
		D3DXVECTOR3 vec(x, y, z);
		::D3DXVec3Normalize(&vec, &vec);
		std::vector<float> result;
		result.push_back(vec.x);
		result.push_back(vec.y);
		result.push_back(vec.z);
		return result;
	}
}

PYBIND11_MAKE_OPAQUE(std::vector<float>);
PYBIND11_MAKE_OPAQUE(std::vector<int>);

PYBIND11_PLUGIN(mmdbridge) {
	py::module m("mmdbridge");

	m.def("get_vertex_buffer_size", get_vertex_buffer_size);
	m.def("get_vertex_size", get_vertex_size);
	m.def("get_vertex", get_vertex);
	m.def("get_normal_size", get_normal_size);
	m.def("get_normal", get_normal);
	m.def("get_uv_size", get_uv_size);
	m.def("get_uv", get_uv);
	m.def("get_material_size", get_material_size);
	m.def("is_accessory", is_accessory);
	m.def("get_pre_accessory_count", get_pre_accessory_count);
	m.def("get_ambient", get_ambient);
	m.def("get_diffuse", get_diffuse);
	m.def("get_specular", get_specular);
	m.def("get_emissive", get_emissive);
	m.def("get_power", get_power);
	m.def("get_texture", get_texture);
	m.def("get_exported_texture", get_exported_texture);
	m.def("get_face_size", get_face_size);
	m.def("get_face", get_face);
	m.def("get_texture_buffer_size", get_texture_buffer_size);
	m.def("get_texture_size", get_texture_size);
	m.def("get_texture_name", get_texture_name);
	m.def("get_texture_pixel", get_texture_pixel);
	m.def("get_camera_up", get_camera_up);
	m.def("get_camera_up_org", get_camera_up_org);
	m.def("get_camera_at",  get_camera_at);
	m.def("get_camera_eye",  get_camera_eye);
	m.def("get_camera_eye_org",  get_camera_eye_org);
	m.def("get_camera_fovy", get_camera_fovy);
	m.def("get_camera_aspect", get_camera_aspect);
	m.def("get_camera_near", get_camera_near);
	m.def("get_camera_far", get_camera_far);
	m.def("messagebox", message);
	m.def("export_texture", export_texture);
	m.def("export_textures", export_textures);
	m.def("export_uncopied_textures", export_textures);
	m.def("copy_textures", copy_textures);
	m.def("get_frame_number", get_frame_number);
	m.def("get_start_frame", get_start_frame);
	m.def("get_end_frame", get_end_frame);
	m.def("get_frame_width", get_frame_width);
	m.def("get_frame_height", get_frame_height);
	m.def("get_export_fps", get_export_fps);
	m.def("get_base_path", get_base_path);
	m.def("get_light", get_light);
	m.def("get_light_color", get_light_color);
	m.def("get_buffer_filename", get_buffer_filename);
	m.def("get_accessory_size", get_accessory_size);
	m.def("get_accessory_filename", get_accessory_filename);
	m.def("get_object_size", get_object_size);
	m.def("get_object_filename", get_object_filename);
	m.def("get_bone_size", get_bone_size);
	m.def("get_bone_name", get_bone_name);
	m.def("get_bone_matrix", get_bone_matrix);
	m.def("get_world", get_world);
	m.def("get_world_inv", get_world_inv);
	m.def("get_view", get_view);
	m.def("get_projection", get_projection);
	m.def("set_texture_buffer_enabled", set_texture_buffer_enabled);
	m.def("set_int_value", set_int_value);
	m.def("set_float_value", set_float_value);
	m.def("get_int_value", get_int_value);
	m.def("get_float_value", get_float_value);
	m.def("extract_xyz_degree", extract_xyz_degree);
	m.def("invert_matrix", invert_matrix);
	m.def("d3dx_vec3_normalize", d3dx_vec3_normalize);

	py::bind_vector<std::vector<float>>(m, "VectorFloat");
	py::bind_vector<std::vector<int>>(m, "VectorInt");

	return m.ptr();
}

void run_python_script()
{
	relaod_python_script();
	if (BridgeParameter::instance().mmdbridge_python_script.empty()) { return; }

	if (Py_IsInitialized())
	{
		//
		PyEval_InitThreads();
		Py_InspectFlag = 0;
		
		if (script_call_setting > 1)
		{
			script_call_setting = 0;
		}
	}
	else
	{
		InitAlembic();
		InitVMD();
		InitPMX();
		PyImport_AppendInittab("mmdbridge", PyInit_mmdbridge);
		Py_Initialize();
			
		// 入力引数の設定
		{
			int argc = 1;
			const std::wstring wpath = BridgeParameter::instance().base_path;
			wchar_t *path[] = {
				const_cast<wchar_t*>(wpath.c_str())
			};
			PySys_SetArgv(argc, path);
		}
	}

	try
	{
		// モジュール初期化.
		auto global = py::dict(py::module::import("__main__").attr("__dict__"));
		auto script = BridgeParameter::instance().mmdbridge_python_script;
		// スクリプトの実行.
		auto res = py::eval<py::eval_statements>(
			script.c_str(),
			global);
	}
	catch(py::error_already_set const &ex)
	{
		std::string python_error_string = ex.what();
		::MessageBoxA(NULL, python_error_string.c_str(), "python error", MB_OK);
	}
}
//-----------------------------------------------------------Hook関数ポインタ-----------------------------------------------------------

// Direct3DCreate9
IDirect3D9 *(WINAPI *original_direct3d_create)(UINT)(NULL);

HRESULT (WINAPI *original_direct3d9ex_create)(UINT, IDirect3D9Ex**)(NULL);
HMODULE original_d3d9_module(NULL);
BOOL (WINAPI *original_check_fullscreen)()(NULL);
int (WINAPI *original_d3dperf_begin_event)(D3DCOLOR, LPCWSTR)(NULL);
int (WINAPI *original_d3dperf_end_event)()(NULL);
DWORD (WINAPI *original_d3dperf_get_status)()(NULL);
BOOL (WINAPI *original_d3dperf_query_repeat_frame)()(NULL);
void (WINAPI *original_d3dperf_set_marker)(D3DCOLOR, LPCWSTR)(NULL);
void (WINAPI *original_d3dperf_set_options)(DWORD)(NULL);
void (WINAPI *original_d3dperf_set_region)(D3DCOLOR, LPCWSTR)(NULL);
void (WINAPI *original_debug_set_level)()(NULL);
void (WINAPI *original_debug_set_mute)()(NULL);
void* (WINAPI *original_direct3d_shader_validator_create9)()(NULL);
HRESULT (WINAPI *original_psgp_error)()(NULL);
HRESULT (WINAPI *original_psgp_sample_texture)()(NULL);
// IDirect3D9::CreateDevice
HRESULT (WINAPI *original_create_device)(IDirect3D9*,UINT, D3DDEVTYPE, HWND, DWORD, D3DPRESENT_PARAMETERS*, IDirect3DDevice9**)(NULL);

HRESULT(WINAPI *original_create_deviceex)(IDirect3D9Ex*, UINT, D3DDEVTYPE, HWND, DWORD, D3DPRESENT_PARAMETERS*, D3DDISPLAYMODEEX*, IDirect3DDevice9Ex**)(NULL);
// IDirect3DDevice9::BeginScene
HRESULT (WINAPI *original_begin_scene)(IDirect3DDevice9*)(NULL);
// IDirect3DDevice9::EndScene
HRESULT(WINAPI *original_end_scene)(IDirect3DDevice9*)(NULL);
// IDirect3DDevice9::SetFVF
HRESULT (WINAPI *original_set_fvf)(IDirect3DDevice9*, DWORD);
// IDirect3DDevice9::Clear
HRESULT (WINAPI *original_clear)(IDirect3DDevice9*, DWORD, const D3DRECT*, DWORD, D3DCOLOR, float, DWORD)(NULL);
// IDirect3DDevice9::Present
HRESULT (WINAPI *original_present)(IDirect3DDevice9*, const RECT*, const RECT*, HWND, const RGNDATA *)(NULL);
// IDirect3DDevice9::Reset
HRESULT (WINAPI *original_reset)(IDirect3DDevice9*, D3DPRESENT_PARAMETERS*)(NULL);

// IDirect3DDevice9::BeginStateBlock
// この関数で、lpVtblが修正されるので、lpVtbl書き換えなおす
HRESULT (WINAPI *original_begin_state_block)(IDirect3DDevice9 *)(NULL);

// IDirect3DDevice9::EndStateBlock
// この関数で、lpVtblが修正されるので、lpVtbl書き換えなおす
HRESULT (WINAPI *original_end_state_block)(IDirect3DDevice9*, IDirect3DStateBlock9**)(NULL);

// IDirect3DDevice9::DrawIndexedPrimitive
HRESULT (WINAPI *original_draw_indexed_primitive)(IDirect3DDevice9*, D3DPRIMITIVETYPE, INT, UINT, UINT, UINT, UINT)(NULL);

// IDirect3DDevice9::SetStreamSource
HRESULT (WINAPI *original_set_stream_source)(IDirect3DDevice9*, UINT, IDirect3DVertexBuffer9*, UINT, UINT)(NULL);

// IDirect3DDevice9::SetIndices
HRESULT (WINAPI *original_set_indices)(IDirect3DDevice9*, IDirect3DIndexBuffer9*)(NULL);

// IDirect3DDevice9::CreateVertexBuffer
HRESULT (WINAPI *original_create_vertex_buffer)(IDirect3DDevice9*, UINT, DWORD, DWORD, D3DPOOL, IDirect3DVertexBuffer9**, HANDLE*)(NULL);

// IDirect3DDevice9::SetTexture
HRESULT (WINAPI *original_set_texture)(IDirect3DDevice9*, DWORD, IDirect3DBaseTexture9 *)(NULL);

// IDirect3DDevice9::CreateTexture
HRESULT (WINAPI *original_create_texture)(IDirect3DDevice9*, UINT, UINT, UINT, DWORD, D3DFORMAT, D3DPOOL, IDirect3DTexture9**, HANDLE*)(NULL);
//-----------------------------------------------------------------------------------------------------------------------------

static bool writeTextureToFile(const std::string &texturePath, IDirect3DTexture9 * texture, D3DXIMAGE_FILEFORMAT fileFormat)
{
	TextureBuffers::iterator tit = renderData.textureBuffers.find(texture);
	if(tit != renderData.textureBuffers.end())
	{
		if (texture->lpVtbl) {
			std::wstring wstr = umbase::UMStringUtil::utf16_to_wstring(umbase::UMStringUtil::utf8_to_utf16(texturePath));
			HRESULT res = D3DXSaveTextureToFileW(wstr.c_str(), fileFormat,(LPDIRECT3DBASETEXTURE9) texture, NULL);
			if (res == S_OK)
			{
				return true;
			}
		}
	}
	return false;
}

static bool writeTextureToFiles(const std::string &texturePath, const std::string &textureType, bool uncopied)
{
	bool res = true;

	D3DXIMAGE_FILEFORMAT fileFormat;
	if (textureType == "bmp" || textureType == "BMP") { fileFormat = D3DXIFF_BMP; }
	else if (textureType == "png" || textureType == "PNG") { fileFormat = D3DXIFF_PNG; }
	else if (textureType == "jpg" || textureType == "JPG") { fileFormat = D3DXIFF_JPG; }
	else if (textureType == "tga" || textureType == "TGA") { fileFormat = D3DXIFF_TGA; }
	else if (textureType == "dds" || textureType == "DDS") { fileFormat = D3DXIFF_DDS; }
	else if (textureType == "ppm" || textureType == "PPM") { fileFormat = D3DXIFF_PPM; }
	else if (textureType == "dib" || textureType == "DIB") { fileFormat = D3DXIFF_DIB; }
	else if (textureType == "hdr" || textureType == "HDR") { fileFormat = D3DXIFF_HDR; }
	else if (textureType == "pfm" || textureType == "PFM") { fileFormat = D3DXIFF_PFM; }
	else { return false; }

	char dir[MAX_PATH];
	std::strcpy(dir, texturePath.c_str());
	PathRemoveFileSpecA(dir);
	
	for (size_t i = 0; i <  finishTextureBuffers.size(); ++i)
	{
		IDirect3DTexture9* texture = finishTextureBuffers[i].first;
		bool copied = finishTextureBuffers[i].second;
		if (texture) {
			if (uncopied)
			{
				if (!copied)
				{
					char path[MAX_PATH];
					PathCombineA(path, dir, to_string(texture).c_str());
					if (!writeTextureToFile(std::string(path) + "." + textureType, texture, fileFormat)) { res = false; }
				}
			} else {
				char path[MAX_PATH];
				PathCombineA(path, dir, to_string(texture).c_str());
				if (!writeTextureToFile(std::string(path) + "." + textureType, texture, fileFormat)) { res = false; }
			}
		}
	}

	return res;
}

static bool copyTextureToFiles(const std::u16string &texturePath)
{
	if (texturePath.empty()) return false;

	std::wstring path = umbase::UMStringUtil::utf16_to_wstring(texturePath);
	PathRemoveFileSpec(&path[0]);
	PathAddBackslash(&path[0]);
	if (!PathIsDirectory(path.c_str())) { return false; }
	
	bool res = true;
	for (size_t i = 0; i <  finishTextureBuffers.size(); ++i)
	{
		IDirect3DTexture9* texture = finishTextureBuffers[i].first;
		if (texture) {
			if (!UMCopyTexture(path.c_str(), texture)) { res = false; }
		}
	}
	return res;
}

static bool writeTextureToMemory(const std::string &textureName, IDirect3DTexture9 * texture, bool copied)
{
	// すでにfinishTexutureBufferにあるかどうか
	bool found = false;
	for (size_t i = 0; i < finishTextureBuffers.size(); ++i)
	{
		if (finishTextureBuffers[i].first == texture) { found = true; break; }
	}

	if (!found)
	{
		// 書き出していなかったので書き出しファイルリストに入れる
		std::pair<IDirect3DTexture9*, bool> texturebuffer(texture, copied);
		finishTextureBuffers.push_back(texturebuffer);
	}

	if (BridgeParameter::instance().is_texture_buffer_enabled)
	{
		TextureBuffers::iterator tit = renderData.textureBuffers.find(texture);
		if(tit != renderData.textureBuffers.end())
		{
			// テクスチャをメモリに書き出し
			D3DLOCKED_RECT lockRect;
			HRESULT isLocked = texture->lpVtbl->LockRect(texture, 0, &lockRect, NULL, D3DLOCK_READONLY);
			if (isLocked != D3D_OK) { return false; }
			
			int width = tit->second.wh.x;
			int height = tit->second.wh.y;

			RenderedTexture tex;
			tex.size.x = width;
			tex.size.y = height;
			tex.name = textureName;

			D3DFORMAT format = tit->second.format;
			for(int y = 0; y < height; y++)
			{
				unsigned char *lineHead = (unsigned char*)lockRect.pBits + lockRect.Pitch * y;

				for (int x = 0; x < width; x++)
				{
					if (format == D3DFMT_A8R8G8B8) {
						UMVec4f rgba;
						rgba.x = lineHead[4 * x + 0];
						rgba.y = lineHead[4 * x + 1];
						rgba.z = lineHead[4 * x + 2];
						rgba.w = lineHead[4 * x + 3];
						tex.texture.push_back(rgba);
					} else {
						::MessageBoxA(NULL, std::string("not supported texture format:" + format).c_str(), "info", MB_OK);
					}
				}
			}
			renderedTextures[texture] = tex;

			texture->lpVtbl->UnlockRect(texture, 0);
			return true;
		}
	}
	return false;
}

static HRESULT WINAPI beginScene(IDirect3DDevice9 *device) 
{
	HRESULT res = (*original_begin_scene)(device);
	return res;
}

static HRESULT WINAPI endScene(IDirect3DDevice9 *device)
{
	HRESULT res = (*original_end_scene)(device);
	return res;

}

namespace
{
	static const UINT kMMEListViewId = 1003;
	static const UINT kMMETabControlId = 1002;
	static const UINT kMMEMenuOpenObjectFolder = 49031;
	static const UINT kMMEMenuOpenEffectFolder = 49032;
	static const UINT kMMEMenuMoveModelEarlier = 49033;
	static const UINT kMMEMenuMoveModelLater = 49034;

	struct MMESelectionInfo
	{
		std::wstring object_text;
		std::wstring effect_text;
		std::wstring object_path;
		std::wstring effect_path;
		int pmd_index;
	};

	struct ObjectPathCandidate
	{
		std::wstring path;
		std::wstring file_name;
		std::wstring stem;
		int index;
	};

	static std::wstring acp_to_wstring(const char* src)
	{
		if (!src || !*src) return std::wstring();
		const int size = ::MultiByteToWideChar(CP_ACP, 0, src, -1, NULL, 0);
		if (size <= 0) return std::wstring();
		std::vector<wchar_t> buffer(size);
		::MultiByteToWideChar(CP_ACP, 0, src, -1, &buffer[0], size);
		return std::wstring(&buffer[0]);
	}

	static std::wstring trim_copy(const std::wstring& src)
	{
		const std::wstring spaces(L" \t\r\n");
		const size_t begin = src.find_first_not_of(spaces);
		if (begin == std::wstring::npos) return std::wstring();
		const size_t end = src.find_last_not_of(spaces);
		return src.substr(begin, end - begin + 1);
	}

	static std::wstring normalize_display_text(const std::wstring& src)
	{
		std::wstring text = trim_copy(src);
		if (text.size() >= 2)
		{
			const wchar_t first = text.front();
			const wchar_t last = text.back();
			if ((first == L'"' && last == L'"') || (first == L'\'' && last == L'\''))
			{
				text = trim_copy(text.substr(1, text.size() - 2));
			}
		}
		return text;
	}

	static bool path_exists(const std::wstring& path)
	{
		if (path.empty()) return false;
		return ::GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES;
	}

	static bool is_directory_path(const std::wstring& path)
	{
		if (path.empty()) return false;
		const DWORD attr = ::GetFileAttributesW(path.c_str());
		return attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY) != 0;
	}

	static std::wstring path_file_name_copy(const std::wstring& path)
	{
		if (path.empty()) return std::wstring();
		return std::wstring(::PathFindFileNameW(path.c_str()));
	}

	static std::wstring path_stem_copy(const std::wstring& path)
	{
		const std::wstring file_name = path_file_name_copy(path);
		const size_t dot = file_name.find_last_of(L'.');
		return dot == std::wstring::npos ? file_name : file_name.substr(0, dot);
	}

	static std::wstring directory_from_path(const std::wstring& path)
	{
		if (path.empty()) return std::wstring();
		std::vector<wchar_t> buffer(path.begin(), path.end());
		buffer.push_back(L'\0');
		if (!::PathRemoveFileSpecW(&buffer[0])) return std::wstring();
		return std::wstring(&buffer[0]);
	}

	static std::wstring combine_path(const std::wstring& base, const std::wstring& leaf)
	{
		if (base.empty()) return leaf;
		wchar_t buffer[MAX_PATH * 4] = {};
		if (!::PathCombineW(buffer, base.c_str(), leaf.c_str())) return std::wstring();
		return std::wstring(buffer);
	}

	static std::wstring resolve_runtime_path(const std::wstring& raw_path)
	{
		const std::wstring normalized = normalize_display_text(raw_path);
		if (normalized.empty()) return std::wstring();
		if (!::PathIsRelativeW(normalized.c_str()))
		{
			return path_exists(normalized) ? normalized : std::wstring();
		}
		const std::wstring combined = combine_path(BridgeParameter::instance().base_path, normalized);
		return path_exists(combined) ? combined : std::wstring();
	}

	static bool is_special_effect_name(const std::wstring& text)
	{
		const std::wstring normalized = normalize_display_text(text);
		return normalized.empty()
			|| ::_wcsicmp(normalized.c_str(), L"(none)") == 0
			|| ::_wcsicmp(normalized.c_str(), L"(hide)") == 0
			|| ::_wcsicmp(normalized.c_str(), L"(default)") == 0
			|| ::_wcsicmp(normalized.c_str(), L"default") == 0;
	}

	static std::string get_listview_item_text(HWND list_view, int item, int sub_item)
	{
		char buffer[1024] = {};
		LVITEMA lvitem = {};
		lvitem.iSubItem = sub_item;
		lvitem.cchTextMax = sizeof(buffer);
		lvitem.pszText = buffer;
		::SendMessageA(list_view, LVM_GETITEMTEXTA, static_cast<WPARAM>(item), reinterpret_cast<LPARAM>(&lvitem));
		return std::string(buffer);
	}

	static std::wstring get_listview_item_text_w(HWND list_view, int item, int sub_item)
	{
		return acp_to_wstring(get_listview_item_text(list_view, item, sub_item).c_str());
	}

	static std::vector<ObjectPathCandidate> collect_object_path_candidates()
	{
		std::vector<ObjectPathCandidate> candidates;
		for (int i = 0; i < ExpGetPmdNum(); ++i)
		{
			const std::wstring path = resolve_runtime_path(acp_to_wstring(ExpGetPmdFilename(i)));
			if (!path.empty())
			{
				ObjectPathCandidate candidate = {};
				candidate.path = path;
				candidate.file_name = path_file_name_copy(path);
				candidate.stem = path_stem_copy(path);
				candidate.index = i;
				candidates.push_back(candidate);
			}
		}
		for (int i = 0; i < ExpGetAcsNum(); ++i)
		{
			const std::wstring path = resolve_runtime_path(acp_to_wstring(ExpGetAcsFilename(i)));
			if (!path.empty())
			{
				ObjectPathCandidate candidate = {};
				candidate.path = path;
				candidate.file_name = path_file_name_copy(path);
				candidate.stem = path_stem_copy(path);
				candidate.index = i;
				candidates.push_back(candidate);
			}
		}
		return candidates;
	}

	static std::vector<ObjectPathCandidate> collect_pmd_path_candidates()
	{
		std::vector<ObjectPathCandidate> candidates;
		for (int i = 0; i < ExpGetPmdNum(); ++i)
		{
			const std::wstring path = resolve_runtime_path(acp_to_wstring(ExpGetPmdFilename(i)));
			if (!path.empty())
			{
				ObjectPathCandidate candidate = {};
				candidate.path = path;
				candidate.file_name = path_file_name_copy(path);
				candidate.stem = path_stem_copy(path);
				candidate.index = i;
				candidates.push_back(candidate);
			}
		}
		return candidates;
	}

	static int score_object_path_candidate(const std::wstring& object_text, const ObjectPathCandidate& candidate)
	{
		if (object_text.empty()) return 0;
		if (::PathIsRelativeW(object_text.c_str()) == FALSE && ::_wcsicmp(object_text.c_str(), candidate.path.c_str()) == 0) return 1000;
		if (!candidate.file_name.empty() && ::_wcsicmp(object_text.c_str(), candidate.file_name.c_str()) == 0) return 900;
		if (!candidate.stem.empty() && ::_wcsicmp(object_text.c_str(), candidate.stem.c_str()) == 0) return 850;
		if (!candidate.file_name.empty() && object_text.find(candidate.file_name) != std::wstring::npos) return 700 + static_cast<int>(candidate.file_name.size());
		if (!candidate.stem.empty() && object_text.find(candidate.stem) != std::wstring::npos) return 600 + static_cast<int>(candidate.stem.size());
		return 0;
	}

	static std::wstring resolve_object_path_from_text(const std::wstring& object_text)
	{
		const std::wstring normalized = normalize_display_text(object_text);
		if (normalized.empty()) return std::wstring();

		const std::wstring direct_path = resolve_runtime_path(normalized);
		if (!direct_path.empty()) return direct_path;

		const std::vector<ObjectPathCandidate> candidates = collect_object_path_candidates();
		int best_score = 0;
		std::wstring best_path;
		for (size_t i = 0; i < candidates.size(); ++i)
		{
			const int score = score_object_path_candidate(normalized, candidates[i]);
			if (score > best_score)
			{
				best_score = score;
				best_path = candidates[i].path;
			}
		}
		return best_path;
	}

	static int resolve_pmd_index_from_text(const std::wstring& object_text)
	{
		const std::wstring normalized = normalize_display_text(object_text);
		if (normalized.empty()) return -1;

		const std::wstring direct_path = resolve_runtime_path(normalized);
		const std::vector<ObjectPathCandidate> candidates = collect_pmd_path_candidates();
		int best_score = 0;
		int best_index = -1;
		for (size_t i = 0; i < candidates.size(); ++i)
		{
			if (!direct_path.empty() && ::_wcsicmp(direct_path.c_str(), candidates[i].path.c_str()) == 0)
			{
				return candidates[i].index;
			}
			const int score = score_object_path_candidate(normalized, candidates[i]);
			if (score > best_score)
			{
				best_score = score;
				best_index = candidates[i].index;
			}
		}
		return best_index;
	}

	static BYTE* get_pmd_internal_order_field(int pmd_index)
	{
		if (pmd_index < 0) return NULL;

		HMODULE module = ::GetModuleHandleW(NULL);
		if (!module) return NULL;

		BYTE* base = reinterpret_cast<BYTE*>(module);
		BYTE* root = *reinterpret_cast<BYTE**>(base + 0x1445F8);
		if (!root) return NULL;

		BYTE** pmd_slots = reinterpret_cast<BYTE**>(root + 0xBE8);
		int current = -1;
		for (int i = 0; i < 0xFF; ++i)
		{
			if (pmd_slots[i])
			{
				++current;
				if (current == pmd_index)
				{
					return pmd_slots[i] + 0x3108;
				}
			}
		}
		return NULL;
	}

	static int find_pmd_index_by_internal_order(BYTE order)
	{
		const int count = ExpGetPmdNum();
		for (int i = 0; i < count; ++i)
		{
			BYTE* order_field = get_pmd_internal_order_field(i);
			if (order_field && *order_field == order)
			{
				return i;
			}
		}
		return -1;
	}

	static bool can_move_pmd_internal_order(int pmd_index, int direction)
	{
		BYTE* order_field = get_pmd_internal_order_field(pmd_index);
		if (!order_field) return false;

		const int order = *order_field;
		const int next_order = order + direction;
		if (next_order < 0 || next_order >= ExpGetPmdNum()) return false;
		return find_pmd_index_by_internal_order(static_cast<BYTE>(next_order)) >= 0;
	}

	static bool move_pmd_internal_order(int pmd_index, int direction)
	{
		BYTE* order_field = get_pmd_internal_order_field(pmd_index);
		if (!order_field) return false;

		const int next_order = *order_field + direction;
		if (next_order < 0 || next_order >= ExpGetPmdNum()) return false;

		const int other_index = find_pmd_index_by_internal_order(static_cast<BYTE>(next_order));
		BYTE* other_order_field = get_pmd_internal_order_field(other_index);
		if (!other_order_field) return false;

		const BYTE old_order = *order_field;
		*order_field = *other_order_field;
		*other_order_field = old_order;
		return true;
	}

	static std::wstring resolve_effect_path_from_text(const std::wstring& effect_text, const std::wstring& object_path)
	{
		const std::wstring normalized = normalize_display_text(effect_text);
		if (is_special_effect_name(normalized)) return std::wstring();

		const std::wstring direct_path = resolve_runtime_path(normalized);
		if (!direct_path.empty()) return direct_path;

		if (!object_path.empty())
		{
			const std::wstring object_dir = directory_from_path(object_path);
			const std::wstring object_relative = combine_path(object_dir, normalized);
			if (path_exists(object_relative)) return object_relative;
		}

		if (normalized.find(L'.') == std::wstring::npos)
		{
			return resolve_effect_path_from_text(normalized + L".fx", object_path);
		}

		return std::wstring();
	}

	static bool open_path_in_explorer(const std::wstring& path)
	{
		if (path.empty() || !path_exists(path)) return false;
		std::wstring command = is_directory_path(path)
			? L"explorer.exe \"" + path + L"\""
			: L"explorer.exe /select,\"" + path + L"\"";
		STARTUPINFOW si = {};
		si.cb = sizeof(si);
		PROCESS_INFORMATION pi = {};
		if (!::CreateProcessW(NULL, &command[0], NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi))
		{
			return false;
		}
		::CloseHandle(pi.hThread);
		::CloseHandle(pi.hProcess);
		return true;
	}

	static MMESelectionInfo get_mme_selection_info(HWND dialog)
	{
		MMESelectionInfo info = {};
		info.pmd_index = -1;
		HWND list_view = ::GetDlgItem(dialog, kMMEListViewId);
		if (!list_view) return info;

		int item = ListView_GetNextItem(list_view, -1, LVNI_SELECTED);
		if (item < 0)
		{
			item = ListView_GetNextItem(list_view, -1, LVNI_FOCUSED);
		}
		if (item < 0) return info;

		info.object_text = normalize_display_text(get_listview_item_text_w(list_view, item, 0));
		info.effect_text = normalize_display_text(get_listview_item_text_w(list_view, item, 1));
		info.object_path = resolve_object_path_from_text(info.object_text);
		info.effect_path = resolve_effect_path_from_text(info.effect_text, info.object_path);
		info.pmd_index = resolve_pmd_index_from_text(info.object_text);
		return info;
	}

	static void update_mme_folder_menu_state(HMENU menu, HWND dialog)
	{
		if (!menu) return;
		if (::GetMenuState(menu, kMMEMenuOpenObjectFolder, MF_BYCOMMAND) == 0xFFFFFFFF
			&& ::GetMenuState(menu, kMMEMenuOpenEffectFolder, MF_BYCOMMAND) == 0xFFFFFFFF)
		{
			return;
		}
		const MMESelectionInfo info = get_mme_selection_info(dialog);
		::EnableMenuItem(menu, kMMEMenuOpenObjectFolder, MF_BYCOMMAND | (info.object_path.empty() ? MF_GRAYED : MF_ENABLED));
		::EnableMenuItem(menu, kMMEMenuOpenEffectFolder, MF_BYCOMMAND | (info.effect_path.empty() ? MF_GRAYED : MF_ENABLED));
		::EnableMenuItem(menu, kMMEMenuMoveModelEarlier, MF_BYCOMMAND | (can_move_pmd_internal_order(info.pmd_index, -1) ? MF_ENABLED : MF_GRAYED));
		::EnableMenuItem(menu, kMMEMenuMoveModelLater, MF_BYCOMMAND | (can_move_pmd_internal_order(info.pmd_index, 1) ? MF_ENABLED : MF_GRAYED));
	}

	static void ensure_mme_folder_menu_items(HMENU dialog_menu)
	{
		if (!dialog_menu) return;
		HMENU edit_menu = ::GetSubMenu(dialog_menu, 1);
		if (!edit_menu) return;
		if (::GetMenuState(edit_menu, kMMEMenuOpenObjectFolder, MF_BYCOMMAND) != 0xFFFFFFFF) return;

		MENUITEMINFOA item = {};
		item.cbSize = sizeof(item);
		item.fMask = MIIM_FTYPE;
		item.fType = MFT_SEPARATOR;
		::InsertMenuItemA(edit_menu, ::GetMenuItemCount(edit_menu), TRUE, &item);

		ZeroMemory(&item, sizeof(item));
		item.cbSize = sizeof(item);
		item.fMask = MIIM_ID | MIIM_FTYPE | MIIM_STRING;
		item.fType = MFT_STRING;
		item.wID = kMMEMenuOpenObjectFolder;
		item.dwTypeData = const_cast<LPSTR>("Open Object Folder");
		::InsertMenuItemA(edit_menu, ::GetMenuItemCount(edit_menu), TRUE, &item);

		item.wID = kMMEMenuOpenEffectFolder;
		item.dwTypeData = const_cast<LPSTR>("Open Effect Folder");
		::InsertMenuItemA(edit_menu, ::GetMenuItemCount(edit_menu), TRUE, &item);

		ZeroMemory(&item, sizeof(item));
		item.cbSize = sizeof(item);
		item.fMask = MIIM_FTYPE;
		item.fType = MFT_SEPARATOR;
		::InsertMenuItemA(edit_menu, ::GetMenuItemCount(edit_menu), TRUE, &item);

		ZeroMemory(&item, sizeof(item));
		item.cbSize = sizeof(item);
		item.fMask = MIIM_ID | MIIM_FTYPE | MIIM_STRING;
		item.fType = MFT_STRING;
		item.wID = kMMEMenuMoveModelEarlier;
		item.dwTypeData = const_cast<LPSTR>("Move Model Earlier");
		::InsertMenuItemA(edit_menu, ::GetMenuItemCount(edit_menu), TRUE, &item);

		item.wID = kMMEMenuMoveModelLater;
		item.dwTypeData = const_cast<LPSTR>("Move Model Later");
		::InsertMenuItemA(edit_menu, ::GetMenuItemCount(edit_menu), TRUE, &item);
	}

	static void notify_mme_tab_changed(HWND tab)
	{
		if (!tab) return;
		HWND dialog = ::GetParent(tab);
		if (!dialog) return;

		NMHDR hdr = {};
		hdr.hwndFrom = tab;
		hdr.idFrom = static_cast<UINT_PTR>(::GetDlgCtrlID(tab));
		hdr.code = TCN_SELCHANGING;
		::SendMessage(dialog, WM_NOTIFY, hdr.idFrom, reinterpret_cast<LPARAM>(&hdr));

		hdr.code = TCN_SELCHANGE;
		::SendMessage(dialog, WM_NOTIFY, hdr.idFrom, reinterpret_cast<LPARAM>(&hdr));
	}

	static bool switch_mme_tab_by_wheel(HWND tab, short delta)
	{
		if (!tab || delta == 0) return false;
		const int count = TabCtrl_GetItemCount(tab);
		if (count <= 1) return false;

		int current = TabCtrl_GetCurSel(tab);
		if (current < 0) current = 0;

		const int direction = delta > 0 ? -1 : 1;
		int next = current + direction;
		if (next < 0) next = count - 1;
		if (next >= count) next = 0;
		if (next == current) return false;

		TabCtrl_SetCurSel(tab, next);
		notify_mme_tab_changed(tab);
		return true;
	}
}

HWND g_hWnd=NULL;	//ウィンドウハンドル
HMENU g_hMenu=NULL;	//メニュー
HMENU g_hBridgeMenu=NULL;
HMENU g_hLanguageMenu=NULL;
HWND g_hFrame = NULL; //フレーム数
HWND g_hMMEffectDialog = NULL;
LONG_PTR g_originalMMEffectDialogWndProc = NULL;
HWND g_hMMEffectTab = NULL;
LONG_PTR g_originalMMEffectTabWndProc = NULL;


static void GetFrame(HWND hWnd)
{
	char text[256];
	::GetWindowTextA(hWnd, text, sizeof(text)/sizeof(text[0]));
		
	ui_frame= atoi(text);
}

static BOOL CALLBACK enumChildWindowsProc(HWND hWnd, LPARAM lParam)
{
	RECT rect;
	GetClientRect(hWnd, &rect);

	WCHAR buf[10];
	GetWindowText(hWnd, buf, 10);

	if (!g_hFrame && rect.right == 48 && rect.bottom == 22)
	{
		g_hFrame = hWnd;
		GetFrame(hWnd);
	}
	if (g_hFrame)
	{
		return FALSE;
	}
	return TRUE;	//continue
}

//乗っ取り対象ウィンドウの検索
static BOOL CALLBACK enumWindowsProc(HWND hWnd,LPARAM lParam)
{
	if (g_hWnd && g_hFrame) {
		GetFrame(g_hFrame);
		return FALSE;
	}
	HANDLE hModule=(HANDLE)GetWindowLongPtr(hWnd, GWLP_HINSTANCE);
	if(GetModuleHandle(NULL)==hModule)
	{
		//自分のプロセスが作ったウィンドウを見つけた
		char szClassName[256];
		GetClassNameA(hWnd,szClassName,sizeof(szClassName)/sizeof(szClassName[0]));

		std::string name(szClassName);

		if (name == "Polygon Movie Maker"){
			g_hWnd = hWnd;
			EnumChildWindows(hWnd, enumChildWindowsProc, 0);
			return FALSE;	//break
		}
	}
	return TRUE;	//continue
}

static BOOL CALLBACK enumMMEffectDialogProc(HWND hWnd, LPARAM lParam)
{
	DWORD process_id = 0;
	::GetWindowThreadProcessId(hWnd, &process_id);
	if (process_id != ::GetCurrentProcessId())
	{
		return TRUE;
	}

	char class_name[256] = {};
	::GetClassNameA(hWnd, class_name, sizeof(class_name) / sizeof(class_name[0]));
	if (std::string(class_name) != "#32770")
	{
		return TRUE;
	}

	if (::GetDlgItem(hWnd, kMMEListViewId) && ::GetDlgItem(hWnd, kMMETabControlId) && ::GetMenu(hWnd))
	{
		g_hMMEffectDialog = hWnd;
		return FALSE;
	}
	return TRUE;
}

static LRESULT CALLBACK mm_effect_dialog_wnd_proc(HWND hWnd, UINT msg, WPARAM wp, LPARAM lp);
static LRESULT CALLBACK mm_effect_tab_wnd_proc(HWND hWnd, UINT msg, WPARAM wp, LPARAM lp);

static void setMyMenu()
{
	if (g_hMenu) return;
	if (g_hWnd)
	{
		HMENU hmenu = GetMenu(g_hWnd);
		HMENU hsubs = CreatePopupMenu();
		HMENU hlangs = CreatePopupMenu();
		int count = GetMenuItemCount(hmenu);
		
		const UiText& text = current_ui_text();
		MENUITEMINFO minfo;
		ZeroMemory(&minfo, sizeof(minfo));
		minfo.cbSize = sizeof(MENUITEMINFO);
		minfo.fMask = MIIM_ID | MIIM_TYPE | MIIM_SUBMENU;
		minfo.fType = MFT_STRING;
		minfo.dwTypeData = TEXT("MMDBridge");
		minfo.hSubMenu = hsubs;

		InsertMenuItem(hmenu, count + 1, TRUE, &minfo);
		ZeroMemory(&minfo, sizeof(minfo));
		minfo.cbSize = sizeof(MENUITEMINFO);
		minfo.fMask = MIIM_ID | MIIM_TYPE;
		minfo.fType = MFT_STRING;
		minfo.dwTypeData = const_cast<LPWSTR>(text.menu_setting);
		minfo.wID = IDC_MENU_PLUGIN_SETTING;
		InsertMenuItem(hsubs, 0, TRUE, &minfo);

		ZeroMemory(&minfo, sizeof(minfo));
		minfo.cbSize = sizeof(MENUITEMINFO);
		minfo.fMask = MIIM_ID | MIIM_TYPE;
		minfo.fType = MFT_STRING;
		minfo.dwTypeData = const_cast<LPWSTR>(text.menu_language_chinese);
		minfo.wID = IDC_MENU_LANGUAGE_ZH_CN;
		InsertMenuItem(hlangs, 0, TRUE, &minfo);

		minfo.dwTypeData = const_cast<LPWSTR>(text.menu_language_source);
		minfo.wID = IDC_MENU_LANGUAGE_SOURCE;
		InsertMenuItem(hlangs, 1, TRUE, &minfo);

		ZeroMemory(&minfo, sizeof(minfo));
		minfo.cbSize = sizeof(MENUITEMINFO);
		minfo.fMask = MIIM_ID | MIIM_TYPE | MIIM_SUBMENU;
		minfo.fType = MFT_STRING;
		minfo.dwTypeData = const_cast<LPWSTR>(text.menu_language);
		minfo.hSubMenu = hlangs;
		InsertMenuItem(hsubs, 1, TRUE, &minfo);

		g_hBridgeMenu = hsubs;
		g_hLanguageMenu = hlangs;
		CheckMenuRadioItem(
			g_hLanguageMenu,
			IDC_MENU_LANGUAGE_ZH_CN,
			IDC_MENU_LANGUAGE_SOURCE,
			ui_language == UiLanguage::Chinese ? IDC_MENU_LANGUAGE_ZH_CN : IDC_MENU_LANGUAGE_SOURCE,
			MF_BYCOMMAND);
		SetMenu(g_hWnd, hmenu);
		g_hMenu = hmenu;
	}
}

static void hookMMEffectDialog()
{
	if (g_hMMEffectDialog && !::IsWindow(g_hMMEffectDialog))
	{
		g_hMMEffectDialog = NULL;
		g_originalMMEffectDialogWndProc = NULL;
		g_hMMEffectTab = NULL;
		g_originalMMEffectTabWndProc = NULL;
	}

	if (!g_hMMEffectDialog)
	{
		::EnumWindows(enumMMEffectDialogProc, 0);
	}
	if (!g_hMMEffectDialog) return;

	HMENU dialog_menu = ::GetMenu(g_hMMEffectDialog);
	ensure_mme_folder_menu_items(dialog_menu);
	update_mme_folder_menu_state(::GetSubMenu(dialog_menu, 1), g_hMMEffectDialog);

	if (!g_originalMMEffectDialogWndProc)
	{
		g_originalMMEffectDialogWndProc = ::GetWindowLongPtr(g_hMMEffectDialog, GWLP_WNDPROC);
		::SetWindowLongPtr(g_hMMEffectDialog, GWLP_WNDPROC, reinterpret_cast<_LONG_PTR>(mm_effect_dialog_wnd_proc));
	}

	HWND tab = ::GetDlgItem(g_hMMEffectDialog, kMMETabControlId);
	if (g_hMMEffectTab && !::IsWindow(g_hMMEffectTab))
	{
		g_hMMEffectTab = NULL;
		g_originalMMEffectTabWndProc = NULL;
	}
	if (tab && tab != g_hMMEffectTab)
	{
		g_hMMEffectTab = tab;
		g_originalMMEffectTabWndProc = NULL;
	}
	if (g_hMMEffectTab && !g_originalMMEffectTabWndProc)
	{
		g_originalMMEffectTabWndProc = ::GetWindowLongPtr(g_hMMEffectTab, GWLP_WNDPROC);
		::SetWindowLongPtr(g_hMMEffectTab, GWLP_WNDPROC, reinterpret_cast<_LONG_PTR>(mm_effect_tab_wnd_proc));
	}
}

LONG_PTR originalWndProc  =NULL;
// このコード モジュールに含まれる関数の宣言を転送します:
INT_PTR CALLBACK DialogProc(HWND, UINT, WPARAM, LPARAM);
HINSTANCE hInstance= NULL;

static void set_menu_item_text_by_id(HMENU menu, UINT id, const wchar_t* text)
{
	if (!menu) return;
	MENUITEMINFO minfo;
	ZeroMemory(&minfo, sizeof(minfo));
	minfo.cbSize = sizeof(MENUITEMINFO);
	minfo.fMask = MIIM_STRING;
	minfo.dwTypeData = const_cast<LPWSTR>(text);
	SetMenuItemInfo(menu, id, FALSE, &minfo);
}

static void set_menu_item_text_by_position(HMENU menu, UINT position, const wchar_t* text)
{
	if (!menu) return;
	MENUITEMINFO minfo;
	ZeroMemory(&minfo, sizeof(minfo));
	minfo.cbSize = sizeof(MENUITEMINFO);
	minfo.fMask = MIIM_STRING;
	minfo.dwTypeData = const_cast<LPWSTR>(text);
	SetMenuItemInfo(menu, position, TRUE, &minfo);
}

static void update_language_menu()
{
	if (!g_hBridgeMenu || !g_hLanguageMenu) return;

	const UiText& text = current_ui_text();
	set_menu_item_text_by_id(g_hBridgeMenu, IDC_MENU_PLUGIN_SETTING, text.menu_setting);
	set_menu_item_text_by_position(g_hBridgeMenu, 1, text.menu_language);
	set_menu_item_text_by_id(g_hLanguageMenu, IDC_MENU_LANGUAGE_ZH_CN, text.menu_language_chinese);
	set_menu_item_text_by_id(g_hLanguageMenu, IDC_MENU_LANGUAGE_SOURCE, text.menu_language_source);
	CheckMenuRadioItem(
		g_hLanguageMenu,
		IDC_MENU_LANGUAGE_ZH_CN,
		IDC_MENU_LANGUAGE_SOURCE,
		ui_language == UiLanguage::Chinese ? IDC_MENU_LANGUAGE_ZH_CN : IDC_MENU_LANGUAGE_SOURCE,
		MF_BYCOMMAND);
	DrawMenuBar(g_hWnd);
}

static void populate_python_script_combo(HWND hCombo)
{
	const BridgeParameter& parameter = BridgeParameter::instance();
	SendMessage(hCombo, CB_RESETCONTENT, 0, 0);
	for (size_t i = 0 ; i < parameter.python_script_name_list.size() ; i++)
	{
		SendMessage(hCombo, CB_ADDSTRING, 0, (LPARAM)parameter.python_script_name_list[i].c_str());
	}
	const LRESULT selected = SendMessage(hCombo, CB_FINDSTRINGEXACT, -1, (LPARAM)parameter.python_script_name.c_str());
	SendMessage(hCombo, CB_SETCURSEL, selected == CB_ERR ? -1 : selected, 0);
}

static void populate_script_call_combo(HWND hCombo)
{
	const UiText& text = current_ui_text();
	const int selected = std::max(0, script_call_setting - 1);
	SendMessage(hCombo, CB_RESETCONTENT, 0, 0);
	SendMessage(hCombo, CB_ADDSTRING, 0, (LPARAM)text.call_setting_options[0]);
	SendMessage(hCombo, CB_ADDSTRING, 0, (LPARAM)text.call_setting_options[1]);
	SendMessage(hCombo, CB_SETCURSEL, selected, 0);
}

static void apply_dialog_language(HWND hWnd)
{
	const UiText& text = current_ui_text();
	SetWindowText(hWnd, text.dialog_title);
	SetDlgItemText(hWnd, IDOK, text.ok);
	SetDlgItemText(hWnd, IDCANCEL, text.cancel);
	SetDlgItemText(hWnd, IDC_STATIC_SCRIPT, text.script_label);
	SetDlgItemText(hWnd, IDC_STATIC_CALL_SETTING, text.call_setting_label);
	SetDlgItemText(hWnd, IDC_BUTTON1, text.rescan);
	SetDlgItemText(hWnd, IDC_STATIC_FRAME_RANGE, text.frame_range_label);
	SetDlgItemText(hWnd, IDC_STATIC_FRAME_SEPARATOR, text.frame_separator);
	SetDlgItemText(hWnd, IDC_STATIC_FPS_HINT, text.fps_hint);
	SetDlgItemText(hWnd, IDC_STATIC_FPS, text.fps);
	populate_script_call_combo(GetDlgItem(hWnd, IDC_COMBO2));
}

static void set_ui_language(UiLanguage language)
{
	if (ui_language == language) return;
	ui_language = language;
	update_language_menu();
}

static LRESULT CALLBACK overrideWndProc(HWND hWnd, UINT msg, WPARAM wp, LPARAM lp)
{
	switch( msg )
	{
		case WM_COMMAND:
		{
			switch(LOWORD(wp))
			{
			case IDC_MENU_PLUGIN_SETTING: // プラグイン設定
				if(hInstance)
				{
					::DialogBox(hInstance, TEXT("IDD_DIALOG1"), NULL,  DialogProc);
				}
				break;
			case IDC_MENU_LANGUAGE_ZH_CN:
				set_ui_language(UiLanguage::Chinese);
				break;
			case IDC_MENU_LANGUAGE_SOURCE:
				set_ui_language(UiLanguage::Source);
				break;
			}
		}
		break;
		case WM_DESTROY:
		break;
	}

	// サブクラスで処理しなかったメッセージは、本来のウィンドウプロシージャに処理してもらう
	return CallWindowProc( (WNDPROC)originalWndProc, hWnd, msg, wp, lp );
}

static LRESULT CALLBACK mm_effect_dialog_wnd_proc(HWND hWnd, UINT msg, WPARAM wp, LPARAM lp)
{
	const LONG_PTR original_wnd_proc = g_originalMMEffectDialogWndProc;
	switch (msg)
	{
	case WM_INITMENUPOPUP:
		update_mme_folder_menu_state(reinterpret_cast<HMENU>(wp), hWnd);
		break;
	case WM_COMMAND:
		switch (LOWORD(wp))
		{
		case kMMEMenuOpenObjectFolder:
		{
			const MMESelectionInfo info = get_mme_selection_info(hWnd);
			if (!info.object_path.empty())
			{
				open_path_in_explorer(info.object_path);
			}
			return 0;
		}
		case kMMEMenuOpenEffectFolder:
		{
			const MMESelectionInfo info = get_mme_selection_info(hWnd);
			if (!info.effect_path.empty())
			{
				open_path_in_explorer(info.effect_path);
			}
			return 0;
		}
		case kMMEMenuMoveModelEarlier:
		{
			const MMESelectionInfo info = get_mme_selection_info(hWnd);
			if (move_pmd_internal_order(info.pmd_index, -1))
			{
				return 0;
			}
			break;
		}
		case kMMEMenuMoveModelLater:
		{
			const MMESelectionInfo info = get_mme_selection_info(hWnd);
			if (move_pmd_internal_order(info.pmd_index, 1))
			{
				return 0;
			}
			break;
		}
		}
		break;
	case WM_NCDESTROY:
		if (g_hMMEffectDialog == hWnd)
		{
			g_hMMEffectDialog = NULL;
			g_originalMMEffectDialogWndProc = NULL;
			g_hMMEffectTab = NULL;
			g_originalMMEffectTabWndProc = NULL;
		}
		break;
	}
	return original_wnd_proc
		? CallWindowProc(reinterpret_cast<WNDPROC>(original_wnd_proc), hWnd, msg, wp, lp)
		: DefWindowProc(hWnd, msg, wp, lp);
}

static LRESULT CALLBACK mm_effect_tab_wnd_proc(HWND hWnd, UINT msg, WPARAM wp, LPARAM lp)
{
	const LONG_PTR original_wnd_proc = g_originalMMEffectTabWndProc;
	switch (msg)
	{
	case WM_MOUSEWHEEL:
		if (switch_mme_tab_by_wheel(hWnd, GET_WHEEL_DELTA_WPARAM(wp)))
		{
			return 0;
		}
		break;
	case WM_NCDESTROY:
		if (g_hMMEffectTab == hWnd)
		{
			g_hMMEffectTab = NULL;
			g_originalMMEffectTabWndProc = NULL;
		}
		break;
	}
	return original_wnd_proc
		? CallWindowProc(reinterpret_cast<WNDPROC>(original_wnd_proc), hWnd, msg, wp, lp)
		: DefWindowProc(hWnd, msg, wp, lp);
}

static INT_PTR CALLBACK DialogProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	const BridgeParameter& parameter = BridgeParameter::instance();
	BridgeParameter& mutable_parameter = BridgeParameter::mutable_instance();
	HWND hCombo1 = GetDlgItem(hWnd, IDC_COMBO1);
	HWND hCombo2 = GetDlgItem(hWnd, IDC_COMBO2);
	HWND hEdit1 = GetDlgItem(hWnd, IDC_EDIT1);
	HWND hEdit2 = GetDlgItem(hWnd, IDC_EDIT2);
	HWND hEdit5 = GetDlgItem(hWnd, IDC_EDIT5);
	switch (msg) {
		case WM_INITDIALOG:
			{
				populate_python_script_combo(hCombo1);
				apply_dialog_language(hWnd);

				::SetWindowTextA(hEdit1, to_string(parameter.start_frame).c_str());
				::SetWindowTextA(hEdit2, to_string(parameter.end_frame).c_str());
				::SetWindowTextA(hEdit5, to_string(parameter.export_fps).c_str());
			}
			return TRUE;
		case WM_CLOSE:
			EndDialog(hWnd, IDCANCEL);
			break;
		case WM_COMMAND:
			switch (LOWORD(wParam))
			{
				case IDOK: // ボタンが押されたとき
					{
						UINT num1 = (UINT)SendMessage(hCombo1, CB_GETCURSEL, 0, 0);
						if (num1 < parameter.python_script_name_list.size())
						{
							if (pythonName != parameter.python_script_name_list[num1])
							{
								pythonName = parameter.python_script_name_list[num1];
								mutable_parameter.python_script_name = parameter.python_script_name_list[num1];
								mutable_parameter.python_script_path = parameter.python_script_path_list[num1];
								relaod_python_script();
							}
						}
						UINT num2 = (UINT)SendMessage(hCombo2, CB_GETCURSEL, 0, 0);
						if (num2 < 2)
						{
							script_call_setting = num2 + 1;
						}

						char text1[32];
						char text2[32];
						char text5[32];
						::GetWindowTextA(hEdit1, text1, sizeof(text1)/sizeof(text1[0]));
						::GetWindowTextA(hEdit2, text2, sizeof(text2)/sizeof(text2[0]));
						::GetWindowTextA(hEdit5, text5, sizeof(text5)/sizeof(text5[0]));
						mutable_parameter.start_frame = atoi(text1);
						mutable_parameter.end_frame = atoi(text2);
						mutable_parameter.export_fps = atof(text5);
						
						if (parameter.start_frame >= parameter.end_frame)
						{
							mutable_parameter.end_frame = parameter.start_frame + 1;
							::SetWindowTextA(hEdit2, to_string(parameter.end_frame).c_str());
						}
						EndDialog(hWnd, IDOK);
					}
					break;
				case IDCANCEL:
					EndDialog(hWnd, IDCANCEL);
					break;
				case IDC_BUTTON1: // 再検索
					reload_python_file_paths();
					populate_python_script_combo(hCombo1);
					break;
			}
			break;
		return TRUE;
	}
	return FALSE;
}

//ウィンドウの乗っ取り
static void overrideGLWindow()
{
	EnumWindows(enumWindowsProc,0);
	setMyMenu();
	// サブクラス化
	if(g_hWnd && !originalWndProc){
		originalWndProc = GetWindowLongPtr(g_hWnd,GWLP_WNDPROC);
		SetWindowLongPtr(g_hWnd,GWLP_WNDPROC,(_LONG_PTR)overrideWndProc);
	}
	hookMMEffectDialog();
}


static bool IsValidCallSetting() { 
	return (script_call_setting == 0) || (script_call_setting == 1);
}

static bool IsValidFrame() {
	HWND recWindow = FindWindowA("RecWindow", NULL);
	return (recWindow != NULL);
}

static bool IsValidTechniq() {
	const int technic = ExpGetCurrentTechnic();
	return (technic == 0 || technic == 1 || technic == 2);
}

static HRESULT WINAPI present(
	IDirect3DDevice9 *device, 
	const RECT * pSourceRect, 
	const RECT * pDestRect, 
	HWND hDestWindowOverride, 
	const RGNDATA * pDirtyRegion)
{
	const float time = ExpGetFrameTime();

	if (pDestRect)
	{
		BridgeParameter::mutable_instance().frame_width = pDestRect->right - pDestRect->left;
		BridgeParameter::mutable_instance().frame_height = pDestRect->bottom - pDestRect->top;
	}
	BridgeParameter::mutable_instance().is_exporting_without_mesh = false;
	overrideGLWindow();
	const bool validFrame = IsValidFrame();
	const bool validCallSetting = IsValidCallSetting();
	const bool validTechniq = IsValidTechniq();
	if (validFrame && validCallSetting && validTechniq)
	{
		if (script_call_setting == 1)
		{
			const BridgeParameter& parameter = BridgeParameter::instance();
			int frame = static_cast<int>(time * BridgeParameter::instance().export_fps + 0.5f);
			if (frame >= parameter.start_frame && frame <= parameter.end_frame)
			{
				if (exportedFrames.find(frame) == exportedFrames.end())
				{
					process_frame = frame;
					run_python_script();
					exportedFrames[process_frame] = 1;
					if (process_frame == parameter.end_frame)
					{
						exportedFrames.clear();
					}
					pre_frame = frame;
				}
			}
		}
		BridgeParameter::mutable_instance().finish_buffer_list.clear();
		presentCount++;
	}
	HRESULT res = (*original_present)(device, pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion);
	return res;
}

static HRESULT WINAPI reset(IDirect3DDevice9 *device, D3DPRESENT_PARAMETERS* pPresentationParameters)
{
	HRESULT res = (*original_reset)(device, pPresentationParameters);
	const UiText& text = current_ui_text();
	::MessageBox(NULL, text.reset_message, text.reset_caption, MB_OK);
	return res;
}

static HRESULT WINAPI setFVF(IDirect3DDevice9 *device, DWORD fvf)
{
	HRESULT res = (*original_set_fvf)(device, fvf);

	if (script_call_setting != 2)
	{
		renderData.fvf = fvf;
		DWORD pos = (fvf & D3DFVF_POSITION_MASK);
		renderData.pos = (pos > 0);
		renderData.pos_xyz	= ((pos & D3DFVF_XYZ) > 0);
		renderData.pos_rhw	= ((pos & D3DFVF_XYZRHW) > 0);
		renderData.pos_xyzb[0] = ((fvf & D3DFVF_XYZB1) == D3DFVF_XYZB1);
		renderData.pos_xyzb[1] = ((fvf & D3DFVF_XYZB2) == D3DFVF_XYZB2);
		renderData.pos_xyzb[2] = ((fvf & D3DFVF_XYZB3) == D3DFVF_XYZB3);
		renderData.pos_xyzb[3] = ((fvf & D3DFVF_XYZB4) == D3DFVF_XYZB4);
		renderData.pos_xyzb[4] = ((fvf & D3DFVF_XYZB5) == D3DFVF_XYZB5);
		renderData.pos_last_beta_ubyte4 = ((fvf & D3DFVF_LASTBETA_UBYTE4) > 0);
		renderData.normal	= ((fvf & D3DFVF_NORMAL) > 0);
		renderData.psize	= ((fvf & D3DFVF_PSIZE) > 0);
		renderData.diffuse	= ((fvf & D3DFVF_DIFFUSE) > 0);
		renderData.specular	= ((fvf & D3DFVF_SPECULAR) > 0);
		renderData.texcount	= (fvf & D3DFVF_TEXCOUNT_MASK) >> D3DFVF_TEXCOUNT_SHIFT;
	}

	return res;
}

static void setFVF(DWORD fvf)
{
	renderData.fvf = fvf;

	DWORD pos = (fvf & D3DFVF_POSITION_MASK);
	renderData.pos = (pos > 0);
	renderData.pos_xyz	= ((pos & D3DFVF_XYZ) > 0);
	renderData.pos_rhw	= ((pos & D3DFVF_XYZRHW) > 0);
	renderData.pos_xyzb[0] = ((fvf & D3DFVF_XYZB1) == D3DFVF_XYZB1);
	renderData.pos_xyzb[1] = ((fvf & D3DFVF_XYZB2) == D3DFVF_XYZB2);
	renderData.pos_xyzb[2] = ((fvf & D3DFVF_XYZB3) == D3DFVF_XYZB3);
	renderData.pos_xyzb[3] = ((fvf & D3DFVF_XYZB4) == D3DFVF_XYZB4);
	renderData.pos_xyzb[4] = ((fvf & D3DFVF_XYZB5) == D3DFVF_XYZB5);
	renderData.pos_last_beta_ubyte4 = ((fvf & D3DFVF_LASTBETA_UBYTE4) > 0);
	renderData.normal	= ((fvf & D3DFVF_NORMAL) > 0);
	renderData.psize	= ((fvf & D3DFVF_PSIZE) > 0);
	renderData.diffuse	= ((fvf & D3DFVF_DIFFUSE) > 0);
	renderData.specular	= ((fvf & D3DFVF_SPECULAR) > 0);
	renderData.texcount	= (fvf & D3DFVF_TEXCOUNT_MASK) >> D3DFVF_TEXCOUNT_SHIFT;

}

HRESULT WINAPI clear(
	IDirect3DDevice9 *device, 
	DWORD count, 
	const D3DRECT *pRects, 
	DWORD flags, 
	D3DCOLOR color, 
	float z, 
	DWORD stencil)
{
	HRESULT res = (*original_clear)(device, count, pRects, flags, color, z, stencil);
	return res;
}

static void getTextureParameter(TextureParameter &param)
{
	TextureSamplers::iterator tit0 = renderData.textureSamplers.find(0);
	TextureSamplers::iterator tit1 = renderData.textureSamplers.find(1);
	TextureSamplers::iterator tit2 = renderData.textureSamplers.find(2);

	param.hasTextureSampler0 = (tit0 != renderData.textureSamplers.end());
	param.hasTextureSampler1 = (tit1 != renderData.textureSamplers.end());
	param.hasTextureSampler2 = (tit2 != renderData.textureSamplers.end());

	if (param.hasTextureSampler1) {
		LPWSTR name = UMGetTextureName(tit1->second);
		param.texture = tit1->second;
		param.textureMemoryName = to_string(param.texture);
		if (name)
		{
			std::wstring wname(name);
			to_string(param.textureName, wname);
		}
		if (UMIsAlphaTexture(param.texture))
		{
			param.hasAlphaTexture = true;
		}
	}
}

// 頂点・法線バッファ・テクスチャをメモリに書き込み
static bool writeBuffersToMemory(IDirect3DDevice9 *device)
{
	const int currentTechnic = ExpGetCurrentTechnic();
	const int currentMaterial = ExpGetCurrentMaterial();
	const int currentObject = ExpGetCurrentObject();

	BYTE *pVertexBuf;
	IDirect3DVertexBuffer9 *pStreamData = renderData.pStreamData;

	VertexBufferList& finishBuffers = BridgeParameter::mutable_instance().finish_buffer_list;
	if (std::find(finishBuffers.begin(), finishBuffers.end(), pStreamData) == finishBuffers.end())
	{
		VertexBuffers::iterator vit = renderData.vertexBuffers.find(pStreamData);
		if(vit != renderData.vertexBuffers.end())
		{
			RenderBufferMap& renderedBuffers = BridgeParameter::mutable_instance().render_buffer_map;
			pStreamData->lpVtbl->Lock(pStreamData, 0, 0, (void**)&pVertexBuf, D3DLOCK_READONLY);

			// FVF取得
			DWORD fvf;
			device->lpVtbl->GetFVF(device, &fvf);
			if (renderData.fvf != fvf)
			{
				setFVF(fvf);
			}

			RenderedBuffer renderedBuffer;

			::D3DXMatrixIdentity(&renderedBuffer.world);
			::D3DXMatrixIdentity(&renderedBuffer.view);
			::D3DXMatrixIdentity(&renderedBuffer.projection);
			::D3DXMatrixIdentity(&renderedBuffer.world_inv);
			device->lpVtbl->GetTransform(device ,D3DTS_WORLD, &renderedBuffer.world);
			device->lpVtbl->GetTransform(device ,D3DTS_VIEW, &renderedBuffer.view);
			device->lpVtbl->GetTransform(device ,D3DTS_PROJECTION, &renderedBuffer.projection);
			
			::D3DXMatrixInverse(&renderedBuffer.world_inv, NULL, &renderedBuffer.world);

			int bytePos = 0;

			if (renderedBuffer.world.m[0][0] == 0 && renderedBuffer.world.m[1][1]== 0 && renderedBuffer.world.m[2][2]== 0)
			{
				return false;
			}

			renderedBuffer.isAccessory = false;
			D3DXMATRIX accesosoryMat;
			for (int i = 0; i < ExpGetAcsNum(); ++i)
			{
				int order = ExpGetAcsOrder(i);
				if (order == currentObject)
				{
					renderedBuffer.isAccessory = true;
					renderedBuffer.accessory = i;
					renderedBuffer.order = i;
					accesosoryMat = ExpGetAcsWorldMat(i);
				}
			}

			if (!renderedBuffer.isAccessory)
			{
				for (int i = 0; i < ExpGetPmdNum(); ++i)
				{
					int order = ExpGetPmdOrder(i);
					if (order == currentObject)
					{
						renderedBuffer.order = i;
						break;
					}
				}
			}

			// 頂点
			if (renderData.pos_xyz)
			{
				int initialVertexSize = renderedBuffer.vertecies.size();
				const int size = (vit->second - bytePos) / renderData.stride;
				renderedBuffer.vertecies.resize(size);
				for (size_t i =  bytePos, n = 0; i < vit->second; i += renderData.stride, ++n)
				{
					D3DXVECTOR3 v;
					memcpy(&v, &pVertexBuf[i], sizeof( D3DXVECTOR3 ));
					if (renderedBuffer.isAccessory)
					{
						D3DXVECTOR4 dst;
						::D3DXVec3Transform(&dst, &v, &accesosoryMat);
						v.x = dst.x;
						v.y = dst.y;
						v.z = dst.z;
					}

					renderedBuffer.vertecies[n] = v;
				}
				bytePos += (sizeof(DWORD) * 3);
			}

			// ウェイト（略）

			// 法線
			if (renderData.normal)
			{
				for (size_t i = bytePos; i < vit->second; i += renderData.stride)
				{
					D3DXVECTOR3 n;
					memcpy(&n, &pVertexBuf[i], sizeof( D3DXVECTOR3 ));
					renderedBuffer.normals.push_back(n);
				}
				bytePos += (sizeof(DWORD) * 3);
			}

			// 頂点カラー
			if (renderData.diffuse)
			{
				for (size_t i = 0; i < vit->second; i += renderData.stride)
				{
					DWORD diffuse;
					memcpy(&diffuse, &pVertexBuf[i], sizeof( DWORD ));
					renderedBuffer.diffuses.push_back(diffuse);

				}
				bytePos += (sizeof(DWORD));
			}

			// UV
			if (renderData.texcount > 0) 
			{
				for (int n = 0; n < renderData.texcount; ++n) 
				{
					for (size_t i = bytePos; i < vit->second; i += renderData.stride) 
					{
						UMVec2f uv;
						memcpy(&uv, &pVertexBuf[i], sizeof( UMVec2f ));
						renderedBuffer.uvs.push_back(uv);
					}
					bytePos += (sizeof(DWORD) * 2);
				}
			}

			
			pStreamData->lpVtbl->Unlock(pStreamData);

			// メモリに保存
			finishBuffers.push_back(pStreamData);
			renderedBuffers[pStreamData] = renderedBuffer;
		}
	}
	return true;
}

static bool writeMaterialsToMemory(TextureParameter & textureParameter)
{
	const int currentTechnic = ExpGetCurrentTechnic();
	const int currentMaterial = ExpGetCurrentMaterial();
	const int currentObject = ExpGetCurrentObject();

	IDirect3DVertexBuffer9 *pStreamData = renderData.pStreamData;
	RenderBufferMap& renderedBuffers = BridgeParameter::mutable_instance().render_buffer_map;
	if (renderedBuffers.find(pStreamData) == renderedBuffers.end())
	{
		return false;
	}

	bool notFoundObjectMaterial = (renderedMaterials.find(currentObject) == renderedMaterials.end());
	if (notFoundObjectMaterial || renderedMaterials[currentObject].find(currentMaterial) == renderedMaterials[currentObject].end())
	{
		// D3DMATERIAL9 取得
		D3DMATERIAL9 material = ExpGetPmdMaterial(currentObject, currentMaterial);
		//p_device->lpVtbl->GetMaterial(p_device, &material);
		
		RenderedMaterial *mat = new RenderedMaterial();
		mat->diffuse.x = material.Diffuse.r;
		mat->diffuse.y = material.Diffuse.g;
		mat->diffuse.z = material.Diffuse.b;
		mat->diffuse.w = material.Diffuse.a;
		mat->specular.x = material.Specular.r;
		mat->specular.y = material.Specular.g;
		mat->specular.z = material.Specular.b;
		mat->ambient.x = material.Ambient.r;
		mat->ambient.y = material.Ambient.g;
		mat->ambient.z = material.Ambient.b;
		mat->emissive.x = material.Emissive.r;
		mat->emissive.y = material.Emissive.g;
		mat->emissive.z = material.Emissive.b;
		mat->power = material.Power;
		
		// シェーダー時
		if (currentTechnic == 2) {
			LPD3DXEFFECT* effect =  UMGetEffect();

			if (effect) {
				D3DXHANDLE current = (*effect)->lpVtbl->GetCurrentTechnique(*effect);
				D3DXHANDLE texHandle1 = (*effect)->lpVtbl->GetTechniqueByName(*effect, "DiffuseBSSphiaTexTec");
				D3DXHANDLE texHandle2 = (*effect)->lpVtbl->GetTechniqueByName(*effect, "DiffuseBSTextureTec");
				D3DXHANDLE texHandle3 = (*effect)->lpVtbl->GetTechniqueByName(*effect, "BShadowSphiaTextureTec");
				D3DXHANDLE texHandle4 = (*effect)->lpVtbl->GetTechniqueByName(*effect, "BShadowTextureTec");

				textureParameter.hasTextureSampler2 = false;
				if (current == texHandle1) {
					//::MessageBoxA(NULL, "1", "transp", MB_OK);
					textureParameter.hasTextureSampler2 = true;
				}
				if (current == texHandle2) {
					//::MessageBoxA(NULL, "2", "transp", MB_OK);
					textureParameter.hasTextureSampler2 = true;
				}
				if (current == texHandle3) {
					//::MessageBoxA(NULL, "3", "transp", MB_OK);
					textureParameter.hasTextureSampler2 = true;
				}
				if (current == texHandle4) {
					//::MessageBoxA(NULL, "4", "transp", MB_OK);
					textureParameter.hasTextureSampler2 = true;
				}

				D3DXHANDLE hEdge = (*effect)->lpVtbl->GetParameterByName(*effect, NULL, "EgColor");
				D3DXHANDLE hDiffuse = (*effect)->lpVtbl->GetParameterByName(*effect, NULL, "MatDifColor");
				D3DXHANDLE hToon = (*effect)->lpVtbl->GetParameterByName(*effect, NULL, "ToonColor");
				D3DXHANDLE hSpecular = (*effect)->lpVtbl->GetParameterByName(*effect, NULL, "SpcColor");
				D3DXHANDLE hTransp = (*effect)->lpVtbl->GetParameterByName(*effect, NULL, "transp");
				
				float edge[4];
				float diffuse[4];
				float specular[4];
				float toon[4];
				BOOL transp;
				(*effect)->lpVtbl->GetFloatArray(*effect, hEdge, edge, 4);
				(*effect)->lpVtbl->GetFloatArray(*effect, hToon, toon, 4);
				(*effect)->lpVtbl->GetFloatArray(*effect, hDiffuse, diffuse, 4);
				(*effect)->lpVtbl->GetFloatArray(*effect, hSpecular, specular, 4);
				(*effect)->lpVtbl->GetBool(*effect, hTransp, &transp);
				mat->diffuse.x = diffuse[0];
				mat->diffuse.y = diffuse[1];
				mat->diffuse.z = diffuse[2];
				mat->diffuse.w = diffuse[3];
				mat->power = specular[3];

				if (specular[0] != 0 || specular[1] != 0 || specular[2] != 0)
				{
					mat->specular.x = specular[0];
					mat->specular.y = specular[1];
					mat->specular.z = specular[2];
				}
			}
		}

		if (renderData.texcount > 0)
		{
			DWORD colorRop0;
			DWORD colorRop1;

			p_device->lpVtbl->GetTextureStageState(p_device, 0, D3DTSS_COLOROP, &colorRop0);
			p_device->lpVtbl->GetTextureStageState(p_device, 1, D3DTSS_COLOROP, &colorRop1);

			if (textureParameter.hasTextureSampler2) {

				mat->tex = textureParameter.texture;
				mat->texture = textureParameter.textureName;
				mat->memoryTexture = textureParameter.textureMemoryName;

				if (!textureParameter.hasAlphaTexture)
				{
					mat->diffuse.w = 1.0f;
				}
			} else	if (textureParameter.hasTextureSampler0 || textureParameter.hasTextureSampler1) {
				if (colorRop0 != D3DTOP_DISABLE && colorRop1 != D3DTOP_DISABLE)
				{
					mat->tex = textureParameter.texture;
					mat->texture = textureParameter.textureName;
					mat->memoryTexture = textureParameter.textureMemoryName;
					if (!textureParameter.hasAlphaTexture)
					{
						mat->diffuse.w = 1.0f;
					}
				}
			}
		}

		RenderedBuffer &renderedBuffer = renderedBuffers[pStreamData];
		if (renderedBuffer.isAccessory)
		{
			D3DMATERIAL9 accessoryMat = ExpGetAcsMaterial(renderedBuffer.accessory, currentMaterial);
			mat->diffuse.x = accessoryMat.Diffuse.r * 10.0f;
			mat->diffuse.y = accessoryMat.Diffuse.g * 10.0f;
			mat->diffuse.z = accessoryMat.Diffuse.b * 10.0f;
			mat->specular.x = accessoryMat.Specular.r * 10.0f;
			mat->specular.y = accessoryMat.Specular.g * 10.0f;
			mat->specular.z = accessoryMat.Specular.b * 10.0f;
			mat->ambient.x = accessoryMat.Ambient.r;
			mat->ambient.y = accessoryMat.Ambient.g;
			mat->ambient.z = accessoryMat.Ambient.b;
			mat->diffuse.w = accessoryMat.Diffuse.a;
			mat->diffuse.w *= ::ExpGetAcsTr(renderedBuffer.accessory);
		}

		renderedBuffer.materials.push_back(mat);
		renderedBuffer.material_map[currentMaterial] = mat;
		renderedMaterials[currentObject][currentMaterial] = mat;
	}
	else
	{
		std::map<int, RenderedMaterial*>& materialMap = renderedMaterials[currentObject];
		renderedBuffers[pStreamData].materials.push_back(materialMap[currentMaterial]);
		renderedBuffers[pStreamData].material_map[currentMaterial] = materialMap[currentMaterial];
		renderedMaterials[currentObject][currentMaterial] = materialMap[currentMaterial];
	}

	if (renderedBuffers[pStreamData].materials.size() > 0) 
	{
		return true;
	}
	else
	{
		return false;
	}
}

static void writeMatrixToMemory(IDirect3DDevice9 *device, RenderedBuffer &dst)
{
	::D3DXMatrixIdentity(&dst.world);
	::D3DXMatrixIdentity(&dst.view);
	::D3DXMatrixIdentity(&dst.projection);
	device->lpVtbl->GetTransform(device ,D3DTS_WORLD, &dst.world);
	device->lpVtbl->GetTransform(device ,D3DTS_VIEW, &dst.view);
	device->lpVtbl->GetTransform(device ,D3DTS_PROJECTION, &dst.projection);
}

static void writeLightToMemory(IDirect3DDevice9 *device, RenderedBuffer &renderedBuffer)
{
	BOOL isLight;
	int lightNumber = 0;			
	 p_device->lpVtbl->GetLightEnable(p_device, lightNumber, &isLight);
	 if (isLight)
	 {
		D3DLIGHT9  light;
		p_device->lpVtbl->GetLight(p_device, lightNumber, &light);
		UMVec3f& umlight = renderedBuffer.light;
		D3DXVECTOR3 v(light.Direction.x, light.Direction.y, light.Direction.z);
		D3DXVECTOR4 dst;
		//D3DXVec3Transform(&dst, &v, &renderedBuffer.world);
		// NOTE: 平行移動成分を潰さなくても、回転するだけの関数がありそうな気がする。
		D3DXMATRIX m = renderedBuffer.world_inv;
		// ugly hack.
		m._41 = m._42 = m._43 = 0; m._14 = m._24 = m._34 = m._44 = 0;
		D3DXVec3Transform(&dst, &v, &m);

		umlight.x = dst.x;
		umlight.y = dst.y;
		umlight.z = dst.z;

		
		// SpecularがMMDのUIで設定した値に一番近い。
		// ただし col * 256.0 / 255.0しないと0～1の範囲にならない。
		// see: http://ch.nicovideo.jp/sovoro_mmd/blomaga/ar319862
		FLOAT s = 256.0f / 255.0f;
		renderedBuffer.light_color.x = light.Specular.r * s;
		renderedBuffer.light_color.y = light.Specular.g * s;
		renderedBuffer.light_color.z = light.Specular.b * s;

		//renderedBuffer.light_diffuse.x = light.Diffuse.r;
		//renderedBuffer.light_diffuse.y = light.Diffuse.g;
		//renderedBuffer.light_diffuse.z = light.Diffuse.b;
		//renderedBuffer.light_diffuse.w = light.Diffuse.a;
		//renderedBuffer.light_specular.x = light.Specular.r;
		//renderedBuffer.light_specular.y = light.Specular.g;
		//renderedBuffer.light_specular.z = light.Specular.b;
		//renderedBuffer.light_specular.w = light.Specular.a;
		//renderedBuffer.light_position.x = light.Position.x;
		//renderedBuffer.light_position.y = light.Position.y;
		//renderedBuffer.light_position.z = light.Position.z;
	}
}

static HRESULT WINAPI drawIndexedPrimitive(
	IDirect3DDevice9 *device, 
	D3DPRIMITIVETYPE type, 
	INT baseVertexIndex, 
	UINT minIndex,
	UINT numVertices, 
	UINT startIndex, 
	UINT primitiveCount)
{
	const int currentMaterial = ExpGetCurrentMaterial();
	const int currentObject = ExpGetCurrentObject();

	const bool validCallSetting = IsValidCallSetting();
	const bool validFrame = IsValidFrame();
	const bool validTechniq = IsValidTechniq();
	const bool validBuffer = (!BridgeParameter::instance().is_exporting_without_mesh);

	if (validBuffer && validCallSetting && validFrame && validTechniq && type == D3DPT_TRIANGLELIST)
	{
		// レンダリング開始
		if (renderData.pIndexData && renderData.pStreamData && renderData.pos_xyz)
		{
			// テクスチャ情報取得
			TextureParameter textureParameter;
			getTextureParameter(textureParameter);

			// テクスチャをメモリに保存
			if (textureParameter.texture)
			{
				if (!textureParameter.textureName.empty())
				{
					writeTextureToMemory(textureParameter.textureName, textureParameter.texture, true);
				}
				else
				{
					writeTextureToMemory(textureParameter.textureName, textureParameter.texture, false);
				}
			}

			// 頂点バッファ・法線バッファ・テクスチャバッファをメモリに書き込み
			if (!writeBuffersToMemory(device))
			{
				return (*original_draw_indexed_primitive)(device, type, baseVertexIndex, minIndex, numVertices, startIndex, primitiveCount);
			}
			
			// マテリアルをメモリに書き込み
			if (!writeMaterialsToMemory(textureParameter))
			{
				return  (*original_draw_indexed_primitive)(device, type, baseVertexIndex, minIndex, numVertices, startIndex, primitiveCount);
			}

			// インデックスバッファをメモリに書き込み
			// 法線がない場合法線を計算
			IDirect3DVertexBuffer9 *pStreamData = renderData.pStreamData;
			IDirect3DIndexBuffer9 *pIndexData = renderData.pIndexData;

			D3DINDEXBUFFER_DESC indexDesc;
			if (pIndexData->lpVtbl->GetDesc(pIndexData, &indexDesc) == D3D_OK)
			{
				void *pIndexBuf;
				if (pIndexData->lpVtbl->Lock(pIndexData, 0, 0, (void**)&pIndexBuf, D3DLOCK_READONLY) == D3D_OK)
				{
					RenderBufferMap& renderedBuffers = BridgeParameter::mutable_instance().render_buffer_map;
					RenderedBuffer &renderedBuffer = renderedBuffers[pStreamData];
					RenderedSurface &renderedSurface = renderedBuffer.material_map[currentMaterial]->surface;
					renderedSurface.faces.clear();

					// 変換行列をメモリに書き込み
					writeMatrixToMemory(device, renderedBuffer);

					// ライトをメモリに書き込み
					writeLightToMemory(device, renderedBuffer);

					// インデックスバッファをメモリに書き込み
					// 法線を修正
					for (size_t i = 0, size = primitiveCount * 3; i < size; i += 3)
					{
						UMVec3i face;
						if (indexDesc.Format == D3DFMT_INDEX16)
						{
							WORD* p = (WORD*)pIndexBuf;
							face.x = static_cast<int>((p[startIndex + i + 0]) + 1);
							face.y = static_cast<int>((p[startIndex + i + 1]) + 1);
							face.z = static_cast<int>((p[startIndex + i + 2]) + 1);
						}
						else
						{
							DWORD* p = (DWORD*)pIndexBuf;
							face.x = static_cast<int>((p[startIndex + i + 0]) + 1);
							face.y = static_cast<int>((p[startIndex + i + 1]) + 1);
							face.z = static_cast<int>((p[startIndex + i + 2]) + 1);
						}
						const int vsize = renderedBuffer.vertecies.size();
						if (face.x > vsize || face.y > vsize || face.z > vsize) {
							continue;
						}
						if (face.x <= 0 || face.y <= 0 || face.z <= 0) {
							continue;
						}
						renderedSurface.faces.push_back(face);
						if (!renderData.normal)
						{
							if (renderedBuffer.normals.size() != vsize)
							{
								renderedBuffer.normals.resize(vsize);
							}
							D3DXVECTOR3 n;
							D3DXVECTOR3 v0 = renderedBuffer.vertecies[face.x - 1];
							D3DXVECTOR3 v1 = renderedBuffer.vertecies[face.y - 1];
							D3DXVECTOR3 v2 = renderedBuffer.vertecies[face.z - 1];
							D3DXVECTOR3 v10 = v1-v0;
							D3DXVECTOR3 v20 = v2-v0;
							::D3DXVec3Cross(&n, &v10, &v20);
							renderedBuffer.normals[face.x - 1] += n;
							renderedBuffer.normals[face.y - 1] += n;
							renderedBuffer.normals[face.z - 1] += n;
						}
						if (!renderData.normal)
						{
							for (size_t i = 0, size = renderedBuffer.normals.size(); i < size; ++i)
							{
								D3DXVec3Normalize(
									&renderedBuffer.normals[i],
									&renderedBuffer.normals[i]);
							}
						}
					}
				}
			}
			pIndexData->lpVtbl->Unlock(pIndexData);
		}
	}

	
	HRESULT res = (*original_draw_indexed_primitive)(device, type, baseVertexIndex, minIndex, numVertices, startIndex, primitiveCount);

	UMSync();
	return res;
}

static HRESULT WINAPI createTexture(
	IDirect3DDevice9* device,
	UINT width,
	UINT height,
	UINT levels,
	DWORD usage,
	D3DFORMAT format,
	D3DPOOL pool,
	IDirect3DTexture9** ppTexture,
	HANDLE* pSharedHandle)
{
	HRESULT res = (*original_create_texture)(device, width, height, levels, usage, format, pool, ppTexture, pSharedHandle);

	TextureInfo info;
	info.wh.x = width;
	info.wh.y = height;
	info.format = format;

	renderData.textureBuffers[*ppTexture] = info;

	
	return res;

}

static HRESULT WINAPI createVertexBuffer(
	IDirect3DDevice9* device,
	UINT length,
	DWORD usage,
	DWORD fvf,
	D3DPOOL pool,
	IDirect3DVertexBuffer9** ppVertexBuffer,
	HANDLE* pHandle)
{
	HRESULT res = (*original_create_vertex_buffer)(device, length, usage, fvf, pool, ppVertexBuffer, pHandle);
	
	renderData.vertexBuffers[*ppVertexBuffer] = length;

	return res;
}

static HRESULT WINAPI setTexture(
	IDirect3DDevice9* device,
	DWORD sampler,	
	IDirect3DBaseTexture9 * pTexture)
{
	if (presentCount == 0) {
		IDirect3DTexture9* texture = reinterpret_cast<IDirect3DTexture9*>(pTexture);
		renderData.textureSamplers[sampler] = texture;
	}

	HRESULT res = (*original_set_texture)(device, sampler, pTexture);

	return res;
}

static HRESULT WINAPI setStreamSource(
	IDirect3DDevice9 *device, 
	UINT streamNumber,
	IDirect3DVertexBuffer9 *pStreamData,
	UINT offsetInBytes,
	UINT stride)
{
	HRESULT res = (*original_set_stream_source)(device, streamNumber, pStreamData, offsetInBytes, stride);
	
	int currentTechnic = ExpGetCurrentTechnic();

	const bool validCallSetting = IsValidCallSetting();
	const bool validFrame = IsValidFrame();
	const bool validTechniq = IsValidTechniq() || currentTechnic == 5;

	if (validCallSetting && validFrame && validTechniq) 
	{
		if (pStreamData) {
			renderData.streamNumber = streamNumber;
			renderData.pStreamData = pStreamData;
			renderData.offsetInBytes = offsetInBytes;
			renderData.stride = stride;
		}
	}

	return res;
}

// IDirect3DDevice9::SetIndices
static HRESULT WINAPI setIndices(IDirect3DDevice9 *device, IDirect3DIndexBuffer9 * pIndexData)
{
	HRESULT res = (*original_set_indices)(device, pIndexData);
			
	int currentTechnic = ExpGetCurrentTechnic();

	const bool validCallSetting = IsValidCallSetting();
	const bool validFrame = IsValidFrame();
	const bool validTechniq =  IsValidTechniq() || currentTechnic == 5;
	if (validCallSetting && validFrame && validTechniq) 
	{
		renderData.pIndexData = pIndexData;
	}
	
	return res;
}

// IDirect3DDevice9::BeginStateBlock
// この関数で、lpVtblが修正されるので、lpVtbl書き換えなおす
static HRESULT WINAPI beginStateBlock(IDirect3DDevice9 *device)
{
	originalDevice();
	HRESULT res = (*original_begin_state_block)(device);
	
	p_device = device;
	hookDevice();

	return res;
}

// IDirect3DDevice9::EndStateBlock
// この関数で、lpVtblが修正されるので、lpVtbl書き換えなおす
static HRESULT WINAPI endStateBlock(IDirect3DDevice9 *device, IDirect3DStateBlock9 **ppSB)
{
	originalDevice();
	HRESULT res = (*original_end_state_block)(device, ppSB);

	p_device = device;
	hookDevice();

	return res;
}

static void hookDevice()
{
	if (p_device) 
	{
		// 書き込み属性付与
		DWORD old_protect;
		VirtualProtect(reinterpret_cast<void *>(p_device->lpVtbl), sizeof(p_device->lpVtbl), PAGE_EXECUTE_READWRITE, &old_protect);
		
		p_device->lpVtbl->BeginScene = beginScene;
		p_device->lpVtbl->EndScene = endScene;
		//p_device->lpVtbl->Clear = clear;
		p_device->lpVtbl->Present = present;
		//p_device->lpVtbl->Reset = reset;
		p_device->lpVtbl->BeginStateBlock = beginStateBlock;
		p_device->lpVtbl->EndStateBlock = endStateBlock;		
		p_device->lpVtbl->SetFVF = setFVF;
		p_device->lpVtbl->DrawIndexedPrimitive = drawIndexedPrimitive;
		p_device->lpVtbl->SetStreamSource = setStreamSource;
		p_device->lpVtbl->SetIndices = setIndices;
		p_device->lpVtbl->CreateVertexBuffer = createVertexBuffer;
		p_device->lpVtbl->SetTexture = setTexture;
		p_device->lpVtbl->CreateTexture = createTexture;
		//p_device->lpVtbl->SetTextureStageState = setTextureStageState;

		// 書き込み属性元に戻す
		VirtualProtect(reinterpret_cast<void *>(p_device->lpVtbl), sizeof(p_device->lpVtbl), old_protect, &old_protect);
	}
}

static void originalDevice()
{
	if (p_device) 
	{
		// 書き込み属性付与
		DWORD old_protect;
		VirtualProtect(reinterpret_cast<void *>(p_device->lpVtbl), sizeof(p_device->lpVtbl), PAGE_EXECUTE_READWRITE, &old_protect);
		
		p_device->lpVtbl->BeginScene = original_begin_scene;
		p_device->lpVtbl->EndScene = original_end_scene;
		//p_device->lpVtbl->Clear = clear;
		p_device->lpVtbl->Present = original_present;
		//p_device->lpVtbl->Reset = reset;
		p_device->lpVtbl->BeginStateBlock = original_begin_state_block;
		p_device->lpVtbl->EndStateBlock = original_end_state_block;		
		p_device->lpVtbl->SetFVF = original_set_fvf;
		p_device->lpVtbl->DrawIndexedPrimitive = original_draw_indexed_primitive;
		p_device->lpVtbl->SetStreamSource = original_set_stream_source;
		p_device->lpVtbl->SetIndices = original_set_indices;
		p_device->lpVtbl->CreateVertexBuffer = original_create_vertex_buffer;
		p_device->lpVtbl->SetTexture = original_set_texture;
		p_device->lpVtbl->CreateTexture = original_create_texture;
		//p_device->lpVtbl->SetTextureStageState = setTextureStageState;

		// 書き込み属性元に戻す
		VirtualProtect(reinterpret_cast<void *>(p_device->lpVtbl), sizeof(p_device->lpVtbl), old_protect, &old_protect);
	}
}

static HRESULT WINAPI createDevice(
	IDirect3D9 *direct3d,
	UINT adapter,
	D3DDEVTYPE type,
	HWND window,
	DWORD flag,
	D3DPRESENT_PARAMETERS *param,
	IDirect3DDevice9 **device) 
{
	HRESULT res = (*original_create_device)(direct3d, adapter, type, window, flag, param, device);
	p_device = (*device);
	
	if (p_device) {
		original_begin_scene = p_device->lpVtbl->BeginScene;
		original_end_scene = p_device->lpVtbl->EndScene;
		//original_clear = p_device->lpVtbl->Clear;
		original_present = p_device->lpVtbl->Present;
		//original_reset = p_device->lpVtbl->Reset;
		original_begin_state_block = p_device->lpVtbl->BeginStateBlock;
		original_end_state_block = p_device->lpVtbl->EndStateBlock;
		original_set_fvf = p_device->lpVtbl->SetFVF;
		original_draw_indexed_primitive = p_device->lpVtbl->DrawIndexedPrimitive;
		original_set_stream_source = p_device->lpVtbl->SetStreamSource;
		original_set_indices = p_device->lpVtbl->SetIndices;
		original_create_vertex_buffer = p_device->lpVtbl->CreateVertexBuffer;
		original_set_texture = p_device->lpVtbl->SetTexture;
		original_create_texture = p_device->lpVtbl->CreateTexture;
		//original_set_texture_stage_state = p_device->lpVtbl->SetTextureStageState;

		hookDevice();
	}
	return res;
}

static HRESULT WINAPI createDeviceEx(
	IDirect3D9Ex *direct3dex,
	UINT adapter,
	D3DDEVTYPE type,
	HWND window,
	DWORD flag,
	D3DPRESENT_PARAMETERS *param,
	D3DDISPLAYMODEEX* displayMode,
	IDirect3DDevice9Ex **device)
{
	HRESULT res = (*original_create_deviceex)(direct3dex, adapter, type, window, flag, param, displayMode, device);
	p_device = reinterpret_cast<IDirect3DDevice9*>(*device);

	if (p_device) {
		original_begin_scene = p_device->lpVtbl->BeginScene;
		//original_clear = p_device->lpVtbl->Clear;
		original_present = p_device->lpVtbl->Present;
		//original_reset = p_device->lpVtbl->Reset;
		original_begin_state_block = p_device->lpVtbl->BeginStateBlock;
		original_end_state_block = p_device->lpVtbl->EndStateBlock;
		original_set_fvf = p_device->lpVtbl->SetFVF;
		original_draw_indexed_primitive = p_device->lpVtbl->DrawIndexedPrimitive;
		original_set_stream_source = p_device->lpVtbl->SetStreamSource;
		original_set_indices = p_device->lpVtbl->SetIndices;
		original_create_vertex_buffer = p_device->lpVtbl->CreateVertexBuffer;
		original_set_texture = p_device->lpVtbl->SetTexture;
		original_create_texture = p_device->lpVtbl->CreateTexture;
		//original_set_texture_stage_state = p_device->lpVtbl->SetTextureStageState;

		hookDevice();
	}
	return res;
}

extern "C" {
	BOOL WINAPI CheckFullscreen()
	{
		return original_check_fullscreen ? (*original_check_fullscreen)() : FALSE;
	}

	int WINAPI D3DPERF_BeginEvent(D3DCOLOR color, LPCWSTR name)
	{
		return original_d3dperf_begin_event ? (*original_d3dperf_begin_event)(color, name) : 0;
	}

	int WINAPI D3DPERF_EndEvent()
	{
		return original_d3dperf_end_event ? (*original_d3dperf_end_event)() : 0;
	}

	DWORD WINAPI D3DPERF_GetStatus()
	{
		return original_d3dperf_get_status ? (*original_d3dperf_get_status)() : 0;
	}

	BOOL WINAPI D3DPERF_QueryRepeatFrame()
	{
		return original_d3dperf_query_repeat_frame ? (*original_d3dperf_query_repeat_frame)() : FALSE;
	}

	void WINAPI D3DPERF_SetMarker(D3DCOLOR color, LPCWSTR name)
	{
		if (original_d3dperf_set_marker) {
			(*original_d3dperf_set_marker)(color, name);
		}
	}

	void WINAPI D3DPERF_SetOptions(DWORD options)
	{
		if (original_d3dperf_set_options) {
			(*original_d3dperf_set_options)(options);
		}
	}

	void WINAPI D3DPERF_SetRegion(D3DCOLOR color, LPCWSTR name)
	{
		if (original_d3dperf_set_region) {
			(*original_d3dperf_set_region)(color, name);
		}
	}

	void WINAPI DebugSetLevel()
	{
		if (original_debug_set_level) {
			(*original_debug_set_level)();
		}
	}

	void WINAPI DebugSetMute()
	{
		if (original_debug_set_mute) {
			(*original_debug_set_mute)();
		}
	}

	void* WINAPI Direct3DShaderValidatorCreate9()
	{
		return original_direct3d_shader_validator_create9 ? (*original_direct3d_shader_validator_create9)() : NULL;
	}

	HRESULT WINAPI PSGPError()
	{
		return original_psgp_error ? (*original_psgp_error)() : E_NOTIMPL;
	}

	HRESULT WINAPI PSGPSampleTexture()
	{
		return original_psgp_sample_texture ? (*original_psgp_sample_texture)() : E_NOTIMPL;
	}

	// 偽Direct3DCreate9
	IDirect3D9 * WINAPI Direct3DCreate9(UINT SDKVersion) {
		IDirect3D9 *direct3d((*original_direct3d_create)(SDKVersion));
		original_create_device = direct3d->lpVtbl->CreateDevice;

		// 書き込み属性付与
		DWORD old_protect;
		VirtualProtect(reinterpret_cast<void *>(direct3d->lpVtbl), sizeof(direct3d->lpVtbl), PAGE_EXECUTE_READWRITE, &old_protect);
		
		direct3d->lpVtbl->CreateDevice = createDevice;

		// 書き込み属性元に戻す
		VirtualProtect(reinterpret_cast<void *>(direct3d->lpVtbl), sizeof(direct3d->lpVtbl), old_protect, &old_protect);

		return direct3d;
	}

	HRESULT WINAPI Direct3DCreate9Ex(UINT SDKVersion, IDirect3D9Ex **ppD3D) {
		IDirect3D9Ex *direct3d9ex = NULL;
		(*original_direct3d9ex_create)(SDKVersion, &direct3d9ex);

		if (direct3d9ex) 
		{
			original_create_deviceex = direct3d9ex->lpVtbl->CreateDeviceEx;
			if (original_create_deviceex)
			{
				// 書き込み属性付与
				DWORD old_protect;
				VirtualProtect(reinterpret_cast<void *>(direct3d9ex->lpVtbl), sizeof(direct3d9ex->lpVtbl), PAGE_EXECUTE_READWRITE, &old_protect);

				direct3d9ex->lpVtbl->CreateDeviceEx = createDeviceEx;

				// 書き込み属性元に戻す
				VirtualProtect(reinterpret_cast<void *>(direct3d9ex->lpVtbl), sizeof(direct3d9ex->lpVtbl), old_protect, &old_protect);

				*ppD3D = direct3d9ex;
				return S_OK;
			}
		}
		return E_ABORT;
	}

} // extern "C"

bool d3d9_initialize()
{
	// MMDフルパスの取得.
	{
		wchar_t app_full_path[1024];
		GetModuleFileName(NULL, app_full_path, sizeof(app_full_path) / sizeof(wchar_t));
		std::wstring path(app_full_path);
		const size_t pos = path.find_last_of(L"\\/");
		BridgeParameter::mutable_instance().base_path = (pos == std::wstring::npos) ? std::wstring() : path.substr(0, pos + 1);
	}

	reload_python_file_paths();
	pythonName = BridgeParameter::instance().python_script_name;
	relaod_python_script();

	std::wstring d3d9_path = BridgeParameter::instance().base_path + _T("d3d9_mme.dll");
	original_d3d9_module = LoadLibrary(d3d9_path.c_str());
	if (!original_d3d9_module) {
		TCHAR system_path_buffer[1024];
		GetSystemDirectory(system_path_buffer, MAX_PATH );
		d3d9_path.assign(system_path_buffer);
		d3d9_path.append(_T("\\D3D9.DLL"));
		original_d3d9_module = LoadLibrary(d3d9_path.c_str());
	}

	if (!original_d3d9_module) {
		return FALSE;
	}

	// オリジナルDirect3DCreate9の関数ポインタを取得
	original_check_fullscreen = reinterpret_cast<BOOL (WINAPI*)()>(GetProcAddress(original_d3d9_module, "CheckFullscreen"));
	original_d3dperf_begin_event = reinterpret_cast<int (WINAPI*)(D3DCOLOR, LPCWSTR)>(GetProcAddress(original_d3d9_module, "D3DPERF_BeginEvent"));
	original_d3dperf_end_event = reinterpret_cast<int (WINAPI*)()>(GetProcAddress(original_d3d9_module, "D3DPERF_EndEvent"));
	original_d3dperf_get_status = reinterpret_cast<DWORD (WINAPI*)()>(GetProcAddress(original_d3d9_module, "D3DPERF_GetStatus"));
	original_d3dperf_query_repeat_frame = reinterpret_cast<BOOL (WINAPI*)()>(GetProcAddress(original_d3d9_module, "D3DPERF_QueryRepeatFrame"));
	original_d3dperf_set_marker = reinterpret_cast<void (WINAPI*)(D3DCOLOR, LPCWSTR)>(GetProcAddress(original_d3d9_module, "D3DPERF_SetMarker"));
	original_d3dperf_set_options = reinterpret_cast<void (WINAPI*)(DWORD)>(GetProcAddress(original_d3d9_module, "D3DPERF_SetOptions"));
	original_d3dperf_set_region = reinterpret_cast<void (WINAPI*)(D3DCOLOR, LPCWSTR)>(GetProcAddress(original_d3d9_module, "D3DPERF_SetRegion"));
	original_debug_set_level = reinterpret_cast<void (WINAPI*)()>(GetProcAddress(original_d3d9_module, "DebugSetLevel"));
	original_debug_set_mute = reinterpret_cast<void (WINAPI*)()>(GetProcAddress(original_d3d9_module, "DebugSetMute"));
	original_direct3d_shader_validator_create9 = reinterpret_cast<void* (WINAPI*)()>(GetProcAddress(original_d3d9_module, "Direct3DShaderValidatorCreate9"));
	original_psgp_error = reinterpret_cast<HRESULT (WINAPI*)()>(GetProcAddress(original_d3d9_module, "PSGPError"));
	original_psgp_sample_texture = reinterpret_cast<HRESULT (WINAPI*)()>(GetProcAddress(original_d3d9_module, "PSGPSampleTexture"));

	original_direct3d_create = reinterpret_cast<IDirect3D9 *(WINAPI*)(UINT)>(GetProcAddress(original_d3d9_module, "Direct3DCreate9"));
	if (!original_direct3d_create) {
		return FALSE;
	}
	original_direct3d9ex_create = reinterpret_cast<HRESULT(WINAPI*)(UINT, IDirect3D9Ex**)>(GetProcAddress(original_d3d9_module, "Direct3DCreate9Ex"));
	if (!original_direct3d9ex_create) {
		return FALSE;
	}

	return TRUE;
}
	
void d3d9_dispose() 
{
	renderData.dispose();
	DisposePMX();
	DisposeVMD();
	DisposeAlembic();
}

// DLLエントリポイント
BOOL APIENTRY DllMain(HINSTANCE hinst, DWORD reason, LPVOID)
{
	switch (reason) 
	{
		case DLL_PROCESS_ATTACH:
			hInstance=hinst;
			d3d9_initialize();
			break;
		case DLL_PROCESS_DETACH:
			d3d9_dispose();
			break;
	}
	return TRUE;
}

