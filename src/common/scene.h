#ifndef _SCENE_H_
#define _SCENE_H_

#include "common.h"
#include "json.h"
#include "vmath.h"
#include "image.h"

// forward declarations
struct BVHAccelerator;

// blinn-phong material
// textures are scaled by the respective coefficient and may be missing
struct Material {
    vec3f       kd = one3f;             // diffuse coefficient
    vec3f       ks = zero3f;            // specular coefficient
    float       n  = 10;                // specular exponent
    vec3f       kr = zero3f;            // reflection coefficient
    vec3f       ke = zero3f;            // emission coefficient
    
    image3f*    kd_txt   = nullptr;     // diffuse texture
    image3f*    ks_txt   = nullptr;     // specular texture
    image3f*    kr_txt   = nullptr;     // reflection texture
    image3f*    norm_txt = nullptr;     // normal texture
    image3f*    ke_txt   = nullptr;     // emission texture
    
    bool        double_sided = false;   // double-sided material
    bool        microfacet   = false;   // use microfacet formulation
};


// indexed mesh data structure with vertex positions and normals,
// a list of indices for triangle and quad faces, material and frame
struct Mesh {
    frame3f         frame = identity_frame3f;   // frame
    vector<vec3f>   pos;                        // vertex position
    vector<vec3f>   norm;                       // vertex normal
    vector<vec2f>   texcoord;                   // vertex texcture coordinates
    vector<vec3i>   triangle;                   // triangle
    vector<vec4i>   quad;                       // quad
    vector<int>     point;                      // point
    vector<vec2i>   line;                       // line
    vector<vec4i>   spline;                     // cubic bezier segments
    Material*       mat = new Material();       // material
    
    int  subdivision_catmullclark_level = 0;        // catmullclark subdiv level
    bool subdivision_catmullclark_smooth = false;   // catmullclark subdiv smooth
    int  subdivision_bezier_level = 0;              // bezier subdiv level
    bool subdivision_bezier_uniform = true;         // bezier subdiv: true=uniform, false=de casteljau
    

};

// surface made of either a sphere or a quad (as determined by
// isquad. the sphere is centered frame.o with radius radius.
// the quad is at frame.o with normal frame.z and axes frame.x, frame.y.
// the quad side is 2*radius.
struct Surface {
    frame3f     frame = identity_frame3f;   // frame
    float       radius = 1;                 // radius
    bool        isquad = false;             // whether it's a quad
    Material*   mat = new Material();       // material

    
    Mesh*       _display_mesh = nullptr;    // display mesh
    int         subdivision_level = 0;
    bool        subdivision_smooth = false;
};

// point light at frame.o with intensity intensity
struct Light {
    frame3f     frame = identity_frame3f;       // frame
    vec3f       intensity = one3f;              // intersntiy
};

// perspective camera at frame.o with direction (-z)
// and image plane orientation (x,y); the image plane
// is at a distance dist with size (width,height);
// the camera is focussed at a distance focus.
struct Camera {
    frame3f frame = identity_frame3f;   // frame
    float   width = 1;                  // image plane width
    float   height = 1;                 // image plane height
    float   dist = 1;                   // image plane distance
    float   focus = 1;                  // distance of focus
};


// scene comprised of a camera, a list of meshes,
// and a list of lights. rendering parameters are
// also included, namely the background color (color
// if a ray misses) the ambient illumination, the
// image resolution (image_width, image_height) and
// the samples per pixel (image_samples).
struct Scene {
    Camera*             camera = new Camera();  // camera
    
    int                 image_width = 512;      // image resolution in x
    int                 image_height = 512;     // image resolution in y
    int                 image_samples = 1;      // samples per pixels in each direction
    
    vector<Light*>      lights;                 // lights
    
    vec3f               background = one3f*0.2; // background color
    vec3f               ambient = one3f*0.2;    // ambient illumination
    
    vector<Surface*>    surfaces;               // surfaces
    vector<Mesh*>       meshes;                 // meshes
    
    
    bool                draw_wireframe = false; // whether to use wireframe for interactive drawing
    bool                draw_animated = false;  // whether to draw with animation
    bool                draw_gpu_skinning = false;  // whether skinning is performed on the gpu
    bool                draw_captureimage = false;  // whether to capture the image in the next frame
    
};

// grab all scene textures
vector<image3f*> get_textures(Scene* scene);

// create a Camera at eye, pointing towards center with up vector up, and with specified image plane params
Camera* lookat_camera(vec3f eye, vec3f center, vec3f up, float width, float height, float dist);

// set camera view with a "turntable" modification
void set_view_turntable(Camera* camera, float rotate_phi, float rotate_theta, float dolly, float pan_x, float pan_y);

// load a scene from a json file
Scene* load_json_scene(const string& filename);

// create test scenes that do not need to be loaded from a file
Scene* create_test_scene(int scene_type);

#endif

