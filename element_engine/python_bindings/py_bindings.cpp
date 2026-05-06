#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/operators.h>
#include "../include/element_engine.hpp"
#include <cstring>
#include <sstream>

namespace py = pybind11;

// -------------------------------------------------------------------------
// Python module: element_engine_cpp
// -------------------------------------------------------------------------
PYBIND11_MODULE(element_engine_cpp, m) {
    m.doc() = "C++ element identification engine for CYLLAMA COMPUSE";

    // ------------------------------------------------------------------
    // ElementRect
    // ------------------------------------------------------------------
    py::class_<ElementRect>(m, "ElementRect")
        .def(py::init([]() {
            ElementRect r{}; r.x = r.y = r.width = r.height = 0; return r;
        }))
        .def_readwrite("x",      &ElementRect::x)
        .def_readwrite("y",      &ElementRect::y)
        .def_readwrite("width",  &ElementRect::width)
        .def_readwrite("height", &ElementRect::height)
        .def("is_valid",  &ElementRect::is_valid)
        .def("is_empty",  &ElementRect::is_empty)
        .def("center_x",  &ElementRect::center_x)
        .def("center_y",  &ElementRect::center_y)
        .def("__repr__", [](const ElementRect& r) {
            std::ostringstream oss;
            oss << "ElementRect(x=" << r.x << ", y=" << r.y
                << ", width=" << r.width << ", height=" << r.height << ")";
            return oss.str();
        });

    // ------------------------------------------------------------------
    // ElementId
    // ------------------------------------------------------------------
    py::class_<ElementId>(m, "ElementId")
        .def(py::init([]() { ElementId id; std::memset(&id, 0, sizeof(id)); return id; }))
        .def_readwrite("process_id",       &ElementId::process_id)
        .def_readwrite("window_handle",    &ElementId::window_handle)
        .def_readwrite("automation_id_hash", &ElementId::automation_id_hash)
        .def_readwrite("role",             &ElementId::role)
        .def_readwrite("checksum",         &ElementId::checksum)
        .def("__eq__", [](const ElementId& a, const ElementId& b) { return a == b; })
        .def("__ne__", [](const ElementId& a, const ElementId& b) { return a != b; })
        .def("__hash__", [](const ElementId& id) {
            return static_cast<py::int_::value_type>(element_id_hash(&id));
        })
        .def("__repr__", [](const ElementId& id) {
            std::ostringstream oss;
            oss << "ElementId(pid=" << id.process_id
                << ", hwnd=" << id.window_handle
                << ", role=" << id.role
                << ", crc=" << id.checksum << ")";
            return oss.str();
        });

    // ------------------------------------------------------------------
    // ElementInfo
    // ------------------------------------------------------------------
    py::class_<ElementInfo>(m, "ElementInfo")
        .def(py::init([]() {
            ElementInfo i; std::memset(&i, 0, sizeof(i)); return i;
        }))
        .def_readwrite("id",          &ElementInfo::id)
        .def_readwrite("rect",        &ElementInfo::rect)
        .def_readwrite("enabled",     &ElementInfo::enabled)
        .def_readwrite("focused",     &ElementInfo::focused)
        .def_readwrite("child_count", &ElementInfo::child_count)
        .def_property("name", [](const ElementInfo& i) {
            return std::string(i.name);
        }, [](ElementInfo& i, const std::string& s) {
            std::strncpy(i.name, s.c_str(), sizeof(i.name) - 1);
        })
        .def_property("role_name", [](const ElementInfo& i) {
            return std::string(i.role_name);
        }, [](ElementInfo& i, const std::string& s) {
            std::strncpy(i.role_name, s.c_str(), sizeof(i.role_name) - 1);
        })
        .def_property("value", [](const ElementInfo& i) {
            return std::string(i.value);
        }, [](ElementInfo& i, const std::string& s) {
            std::strncpy(i.value, s.c_str(), sizeof(i.value) - 1);
        })
        .def("to_dict", [](const ElementInfo& info) -> py::dict {
            py::dict d;
            d["id"] = py::dict(
                py::arg("process_id")       = info.id.process_id,
                py::arg("window_handle")    = info.id.window_handle,
                py::arg("role")             = info.id.role,
                py::arg("checksum")         = info.id.checksum,
                py::arg("automation_id_hash") = info.id.automation_id_hash,
                py::arg("source")           = static_cast<int>(info.id.source)
            );
            d["name"]      = std::string(info.name);
            d["role_name"] = std::string(info.role_name);
            d["value"]     = std::string(info.value);
            d["x"]         = info.rect.center_x();
            d["y"]         = info.rect.center_y();
            d["width"]     = info.rect.width;
            d["height"]    = info.rect.height;
            d["enabled"]   = info.enabled;
            d["focused"]   = info.focused;
            d["source"]    = (info.source == IdSource::Accessibility)
                                ? "accessibility" : "ocr";
            return d;
        })
        .def("__repr__", [](const ElementInfo& info) {
            std::ostringstream oss;
            oss << "ElementInfo(name='" << info.name
                << "', role='" << info.role_name
                << "', x=" << info.rect.x
                << ", y=" << info.rect.y << ")";
            return oss.str();
        });

    // ------------------------------------------------------------------
    // Engine (wraps ElementEngine*)
    // ------------------------------------------------------------------
    py::class_<ElementEngine>(m, "Engine")
        .def(py::init([]() -> ElementEngine* {
            ElementEngine* e = element_engine_create();
            if (!e) throw std::runtime_error("Failed to create element engine");
            return e;
        }), py::return_value_policy::take_ownership)
        .def("enumerate", [](ElementEngine* engine, uint32_t max_elements) {
            std::vector<ElementInfo> result;
            uint32_t count = 0;
            ElementInfo** arr = element_engine_enumerate(engine, max_elements, &count);
            if (arr) {
                result.reserve(count);
                for (uint32_t i = 0; i < count; ++i) {
                    result.push_back(*arr[i]);
                }
                element_info_free_array(arr, count);
            }
            return result;
        }, py::arg("max_elements") = 0)
        .def("get_info", [](ElementEngine* engine, const ElementId& id) -> py::object {
            ElementInfo* info = element_get_info(engine, &id);
            if (!info) return py::none();
            ElementInfo copy = *info;
            std::free(info);
            return py::cast(copy);
        })
        .def("click", [](ElementEngine* engine, const ElementId& id) {
            return element_click(engine, &id);
        })
        .def("type_text", [](ElementEngine* engine, const ElementId& id,
                             const std::string& text) {
            return element_type_text(engine, &id, text.c_str());
        })
        .def("scroll", [](ElementEngine* engine, const ElementId& id,
                          int direction, int amount) {
            return element_scroll(engine, &id, direction, amount);
        })
        .def("flush_cache", [](ElementEngine* engine) {
            element_engine_flush_cache(engine);
        })
        .def("__enter__", [](ElementEngine* e) { return e; })
        .def("__exit__", [](ElementEngine* e, py::object, py::object, py::object) {
            element_engine_destroy(e);
        });
}
