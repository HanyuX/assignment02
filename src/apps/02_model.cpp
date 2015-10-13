#include "common.h"
#include "scene.h"
#include "image.h"
#include "gls.h"
#include "fstream"
#define pi 3.1415926

std::ofstream fout;
string scene_filename;  // scene filename
string image_filename;  // image filename
Scene* scene;           // scene arrays


void uiloop();          // UI loop


// map used to uniquify edges
struct EdgeMap {
    map<pair<int,int>,int>  _edge_map;  // internal map
    vector<vec2i>           _edge_list; // internal list to generate unique ids
    
    // create an edge map for a collection of triangles and quads
    EdgeMap(vector<vec3i> triangle, vector<vec4i> quad) {
        for(auto f : triangle) { _add_edge(f.x,f.y); _add_edge(f.y,f.z); _add_edge(f.z,f.x); }
        for(auto f : quad) { _add_edge(f.x,f.y); _add_edge(f.y,f.z); _add_edge(f.z,f.w); _add_edge(f.w,f.x); }
    }
    
    // internal function to add an edge
    void _add_edge(int i, int j) {
        if(_edge_map.find(make_pair(i,j)) == _edge_map.end()) {
            _edge_map[make_pair(i,j)] = _edge_list.size();
            _edge_map[make_pair(j,i)] = _edge_list.size();
            _edge_list.push_back(vec2i(i,j));
        }
    }
    
    // edge list
    const vector<vec2i>& edges() const { return _edge_list; }
    
    // get an edge from two vertices
    int edge_index(vec2i e) const {
        error_if_not(not (_edge_map.find(make_pair(e.x,e.y)) == _edge_map.end()), "non existing edge");
        return _edge_map.find(make_pair(e.x, e.y))->second;
    }
};


// make normals for each face - duplicates all vertex data
void facet_normals(Mesh* mesh) {
    // allocates new arrays
    auto pos = vector<vec3f>();
    auto norm = vector<vec3f>();
    auto texcoord = vector<vec2f>();
    auto triangle = vector<vec3i>();
    auto quad = vector<vec4i>();
    
    // foreach triangle
    for(auto f : mesh->triangle) {
        // grab current pos size
        auto nv = (int)pos.size();
        // compute face face normal
        auto fn = normalize(cross(mesh->pos[f.y]-mesh->pos[f.x], mesh->pos[f.z]-mesh->pos[f.x]));
        // add triangle
        triangle.push_back({nv,nv+1,nv+2});
        // add vertex data
        for(auto i : range(3)) {
            pos.push_back(mesh->pos[f[i]]);
            norm.push_back(fn);
            if(not mesh->texcoord.empty()) texcoord.push_back(mesh->texcoord[f[i]]);
        }
    }
    
    // foreach quad
    for(auto f : mesh->quad) {
        // grab current pos size
        auto nv = (int)pos.size();
        // compute face normal
        auto fn = normalize(normalize(cross(mesh->pos[f.y]-mesh->pos[f.x], mesh->pos[f.z]-mesh->pos[f.x])) +
                            normalize(cross(mesh->pos[f.z]-mesh->pos[f.x], mesh->pos[f.w]-mesh->pos[f.x])));
        // add quad
        quad.push_back({nv,nv+1,nv+2,nv+3});
        // add vertex data
        for(auto i : range(4)) {
            pos.push_back(mesh->pos[f[i]]);
            norm.push_back(fn);
            if(not mesh->texcoord.empty()) texcoord.push_back(mesh->texcoord[f[i]]);
        }
    }
    
    // set back mesh data
    mesh->pos = pos;
    mesh->norm = norm;
    mesh->texcoord = texcoord;
    mesh->triangle = triangle;
    mesh->quad = quad;
}

// smooth out normal - does not duplicate data
void smooth_normals(Mesh* mesh) {
    // set normals array to the same length as pos and init all elements to zero
    mesh->norm.assign(mesh->pos.size(), zero3f);
    
    put_your_code_here("Implement normal smoothing");
    
    vec3f a = mesh->pos[0];
    for(int i = 0 ; i < mesh->triangle.size() ; ++i){
        vec3f vector1 = mesh->pos[mesh->triangle[i].y] - mesh->pos[mesh->triangle[i].x];
        vec3f vector2 = mesh->pos[mesh->triangle[i].z] - mesh->pos[mesh->triangle[i].x];
        vec3f norm = normalize(cross(vector1,vector2));
        mesh->norm[mesh->triangle[i].x] += norm;
        mesh->norm[mesh->triangle[i].y] += norm;
        mesh->norm[mesh->triangle[i].z] += norm;
    }
    for(int i = 0 ; i < mesh->quad.size() ; ++i){
        vec3f vector1 = mesh->pos[mesh->quad[i].y] - mesh->pos[mesh->quad[i].x];
        vec3f vector2 = mesh->pos[mesh->quad[i].z] - mesh->pos[mesh->quad[i].x];
        vec3f norm = normalize(cross(vector1,vector2));
        mesh->norm[mesh->quad[i].x] += norm;
        mesh->norm[mesh->quad[i].y] += norm;
        mesh->norm[mesh->quad[i].z] += norm;
        mesh->norm[mesh->quad[i].w] += norm;
    }
    for(int i = 0 ; i < mesh->norm.size() ; ++i)
        mesh->norm[i] = normalize(mesh->norm[i]);
    // foreach triangle
        // compute face normal
        // accumulate face normal to the vertex normals of each face index
    // foreach quad
        // compute face normal
        // accumulate face normal to the vertex normals of each face index
    // normalize all vertex normals
}

// smooth out tangents
void smooth_tangents(Mesh* polyline) {
    // set tangent array
    polyline->norm = vector<vec3f>(polyline->pos.size(),zero3f);
    // foreach line
    for(auto l : polyline->line) {
        // compute line tangent
        auto lt = normalize(polyline->pos[l.y]-polyline->pos[l.x]);
        // accumulate segment tangent to vertex tangent on each vertex
        for (auto i : range(2)) polyline->norm[l[i]] += lt;
    }
    // normalize all vertex tangents
    for (auto& t : polyline->norm) t = normalize(t);
}

// subdivide bezier spline into line segments (assume bezier has only bezier segments and no lines)
// subdivide using uniform sampling
void subdivide_bezier_uniform(Mesh *bezier) {
    auto pos = vector<vec3f>();
    auto line = vector<vec2i>();
    // determine number of steps
    int steps = 1 << bezier->subdivision_bezier_level;
    
    for(int i = 0 ; i < bezier->spline.size() ; ++i){
        vec3f p0 = bezier->pos[bezier->spline[i].x];
        vec3f p1 = bezier->pos[bezier->spline[i].y];
        vec3f p2 = bezier->pos[bezier->spline[i].z];
        vec3f p3 = bezier->pos[bezier->spline[i].w];
        int pointL = pos.size();
        for(int j = 0 ; j <= steps ; ++j){
            double t = 1.0/steps * j;
            double b0 = bernstein(t,0,3);
            double b1 = bernstein(t,1,3);
            double b2 = bernstein(t,2,3);
            double b3 = bernstein(t,3,3);
            vec3f newP = p0*b0 + p1*b1 + p2*b2 + p3*b3;

            pos.push_back(newP);
        }

        for(int j = 0 ; j < steps ; ++j){
            line.push_back(vec2i(pointL+j,pointL+j+1));
        }
    }
    
    // foreach spline segment
        // get control points of segment
        // note the starting index of new points
        // foreach step
            // compute t for current segment
            // compute blending weights
            // compute new point position
            // add new point to pos vector
        // foreach step
            // create line segment
    
    // copy vertex positions
    bezier->pos = pos;
    // copy line segments
    bezier->line = line;
    
    // clear bezier array from lines
    bezier->spline.clear();
    bezier->subdivision_bezier_level = 0;
    
    // run smoothing to get proper tangents
    smooth_tangents(bezier);
}

// subdivide bezier spline into line segments (assume bezier has only bezier segments and no lines)
// subdivide using de casteljau algorithm
void subdivide_bezier_decasteljau(Mesh *bezier) {
    auto pos = bezier->pos;
    auto splines = bezier->spline;
    put_your_code_here("Implement de Casteljau algorithm");
    
    // *note*: this psuedocode is for an iterative implementation of the algorithm without adaptive subd
    // foreach level
        // make new arrays of positions and bezier segments
        // copy all the vertices into the new array (this waste space but it is easier for now)
        // foreach bezier segment
            // apply subdivision algorithm
            // prepare indices for two new segments
            // add mid point
            // add points for first segment and fix segment indices
            // add points for second segment and fix segment indices
            // add indices for both segments into new segments array
        // set new arrays pos, segments into the working lineset
    vector<vec3f> tempPos;
    vector<vec4i> tempSpl;
    for(int i = 0 ; i < bezier->subdivision_bezier_level ; ++i){
        tempPos.clear();
        tempSpl.clear();
        for(int j = 0 ; j < splines.size() ; ++j){
            vec3f point1 = (pos[splines[j].x] + pos[splines[j].y])/2;
            vec3f point2 = (pos[splines[j].y] + pos[splines[j].z])/2;
            vec3f point3 = (pos[splines[j].z] + pos[splines[j].w])/2;
            vec3f point12 = (point1 + point2)/2;
            vec3f point23 = (point2 + point3)/2;
            vec3f point13 = (point12+point23)/2;
            tempSpl.push_back(vec4i(tempPos.size()  ,tempPos.size()+1,tempPos.size()+2,tempPos.size()+3));
            tempSpl.push_back(vec4i(tempPos.size()+3,tempPos.size()+4,tempPos.size()+5,tempPos.size()+6));
            tempPos.push_back(pos[splines[j].x]);
            tempPos.push_back(point1);
            tempPos.push_back(point12);
            tempPos.push_back(point13);
            tempPos.push_back(point23);
            tempPos.push_back(point3);
            tempPos.push_back(pos[splines[j].w]);
        }
        pos = tempPos;
        splines = tempSpl;
    }

    // copy vertex positions
    bezier->pos = pos;
    
    // copy bezier segments into line segments
    bezier->line.clear();
    for(auto spline : splines) bezier->line.push_back({spline.x,spline.w});
    
    // clear bezier array from lines
    bezier->spline.clear();
    bezier->subdivision_bezier_level = 0;
    
    // run smoothing to get proper tangents
    smooth_tangents(bezier);
}

// subdivide bezier spline into line segments (assume bezier has only bezier segments and no lines)
void subdivide_bezier(Mesh* bezier) {
    // skip is needed

    if(not bezier->subdivision_bezier_level) return;

    if(bezier->subdivision_bezier_uniform) subdivide_bezier_uniform(bezier);
    else subdivide_bezier_decasteljau(bezier);
}



// apply Catmull-Clark mesh subdivision
// does not subdivide texcoord
void subdivide_catmullclark(Mesh* subdiv) {
    // skip is needed
    if(not subdiv->subdivision_catmullclark_level) return;
    
    // allocate a working Mesh copied from the subdiv
    auto mesh = new Mesh(*subdiv);
    
    image3f image = read_png("/Users/xuehanyu/Desktop/texture.png",false);
    int width = image.width(), height = image.height();
    // foreach level
    for(auto l : range(subdiv->subdivision_catmullclark_level)) {
        // make empty pos and quad arrays
        auto pos = vector<vec3f>();
        auto quad = vector<vec4i>();
        
        // create edge_map from current mesh
        auto edge_map = EdgeMap(mesh->triangle,mesh->quad);

        put_your_code_here("Implement Catmull-Clark Subdivision");
        
        // linear subdivision - create vertices --------------------------------------
        // copy all vertices from the current mesh
        // add vertices in the middle of each edge (use EdgeMap)
        // add vertices in the middle of each triangle
        // add vertices in the middle of each quad
        for(int i = 0 ; i < mesh->pos.size(); ++i)
            pos.push_back(mesh->pos[i]);
        vector<vec2i> edge = edge_map.edges();
        int edgeZ = pos.size();
        for(auto e : edge)
            pos.push_back((mesh->pos[e.x]+mesh->pos[e.y])/2);
        int triZ = pos.size();
        for(auto tri : mesh->triangle)
            pos.push_back((mesh->pos[tri.x] + mesh->pos[tri.y] + mesh->pos[tri.z])/3);
        int quaZ = pos.size();
        for(auto qua : mesh->quad)
            pos.push_back((mesh->pos[qua.x] + mesh->pos[qua.y] + mesh->pos[qua.z] + mesh->pos[qua.w])/4);

        // subdivision pass ----------------------------------------------------------
        // compute an offset for the edge vertices
        // compute an offset for the triangle vertices
        // compute an offset for the quad vertices
        // foreach triangle
            // add three quads to the new quad array
        // foreach quad
            // add four quads to the new quad array
        int i = 0;
        for(auto tri : mesh->triangle){
            int edgeM1 = edgeZ + edge_map.edge_index(vec2i(tri.x,tri.y)),
                    edgeM2 = edgeZ + edge_map.edge_index(vec2i(tri.y,tri.z)),
                        edgeM3 = edgeZ + edge_map.edge_index(vec2i(tri.z,tri.x));
            int cert = triZ + i;
            ++i;
            quad.push_back(vec4i(tri.x,edgeM1,cert,edgeM3));
            quad.push_back(vec4i(tri.y,edgeM2,cert,edgeM1));
            quad.push_back(vec4i(tri.z,edgeM3,cert,edgeM2));
        }
        i = 0;
        for(auto qua : mesh->quad){
            int edgeM1 = edgeZ + edge_map.edge_index(vec2i(qua.x,qua.y)),
                    edgeM2 = edgeZ + edge_map.edge_index(vec2i(qua.y,qua.z)),
                        edgeM3 = edgeZ + edge_map.edge_index(vec2i(qua.z,qua.w)),
                            edgeM4 = edgeZ + edge_map.edge_index(vec2i(qua.w,qua.x));
            int cert = quaZ + i;
            ++i;
            quad.push_back(vec4i(qua.x,edgeM1,cert,edgeM4));
            quad.push_back(vec4i(qua.y,edgeM2,cert,edgeM1));
            quad.push_back(vec4i(qua.z,edgeM3,cert,edgeM2));
            quad.push_back(vec4i(qua.w,edgeM4,cert,edgeM3));
        }

        // averaging pass ------------------------------------------------------------
        // create arrays to compute pos averages (avg_pos, avg_count)
        // arrays have the same length as the new pos array, and are init to zero
        // for each new quad
            // compute quad center using the new pos array
            // foreach vertex index in the quad
                // accumulate face center to the avg_pos and add 1 to avg_count
        // normalize avg_pos with its count avg_count
        vector<int> count(pos.size(),0);
        vector<vec3f> avePoint(pos.size(),vec3f(0,0,0));
        for(auto qua : quad){
            vec3f newPoint = (pos[qua.x] + pos[qua.y] + pos[qua.z] + pos[qua.w]) /4.0; //
            avePoint[qua.x] += newPoint + 10*normalize(avePoint[qua.x]-subdiv->frame.o) * (image.at(fabs(avePoint[qua.x].x),fabs(avePoint[qua.x].y)).x*0.3 + image.at(fabs(avePoint[qua.x].x),fabs(avePoint[qua.x].y)).y * 0.59 + image.at(fabs(avePoint[qua.x].x),fabs(avePoint[qua.x].y)).z * 0.11);
            ++count[qua.x];
            avePoint[qua.y] += newPoint + 10*normalize(avePoint[qua.y]-subdiv->frame.o) * (image.at(fabs(avePoint[qua.y].x),fabs(avePoint[qua.y].y)).x*0.3 + image.at(fabs(avePoint[qua.y].x),fabs(avePoint[qua.y].y)).y * 0.59 + image.at(fabs(avePoint[qua.y].x),fabs(avePoint[qua.y].y)).z * 0.11);
            ++count[qua.y];
            avePoint[qua.z] += newPoint + 10*normalize(avePoint[qua.z]-subdiv->frame.o) * (image.at(fabs(avePoint[qua.z].x),fabs(avePoint[qua.z].y)).x*0.3 + image.at(fabs(avePoint[qua.z].x),fabs(avePoint[qua.z].y)).y * 0.59 + image.at(fabs(avePoint[qua.z].x),fabs(avePoint[qua.z].y)).z * 0.11);
            ++count[qua.z];
            avePoint[qua.w] += newPoint + 10*normalize(avePoint[qua.w]-subdiv->frame.o) * (image.at(fabs(avePoint[qua.w].x),fabs(avePoint[qua.w].y)).x*0.3 + image.at(fabs(avePoint[qua.w].x),fabs(avePoint[qua.w].y)).y * 0.59 + image.at(fabs(avePoint[qua.w].x),fabs(avePoint[qua.w].y)).z * 0.11);
            ++count[qua.w];
        }
        for(int i = 0 ; i < avePoint.size() ; ++i)
            avePoint[i] /= count[i];

//        // correction pass -----------------------------------------------------------
//        // foreach pos, compute correction p = p + (avg_p - p) * (4/avg_count)
        for(int i = 0 ; i < pos.size() ; ++i)
            pos[i] = pos[i] + (avePoint[i] - pos[i]) * (4.0/count[i]);

        // set new arrays pos, quad back into the working mesh; clear triangle array
        mesh->pos = pos;
        mesh->triangle = vector<vec3i>();
        mesh->quad = quad;
    }
    
    // clear subdivision
    mesh->subdivision_catmullclark_level = 0;
    
    // according to smooth, either smooth_normals or facet_normals
    if(subdiv->subdivision_catmullclark_smooth) smooth_normals(mesh);
    else facet_normals(mesh);
    
    // copy back
    *subdiv = *mesh;
    
    // clear
    delete mesh;
}

void subdivide_surface(Surface* surface) {
    // create mesh struct
    auto mesh    = new Mesh{};
    // copy frame
    mesh->frame  = surface->frame;
    // copy material
    mesh->mat    = surface->mat;
    
    // get surface radius
    auto radius  = surface->radius;
    
    // vertexidx is used to look up index of vertex corresponding to (i,j)
    map<pair<int,int>,int> vertexidx;
    
    if(surface->isquad) {
        // compute how much to subdivide
        auto ci = 1 << surface->subdivision_level;
        auto cj = 1 << surface->subdivision_level;
        
        // compute corners of quad
        auto p00 = vec3f(-1,-1,0) * radius;
        auto p01 = vec3f(-1, 1,0) * radius;
        auto p10 = vec3f( 1,-1,0) * radius;
        auto p11 = vec3f( 1, 1,0) * radius;
        
        // foreach column
        for(auto i : range(ci+1)) {
            // foreach row
            for(auto j : range(cj+1)) {
                // compute u,v corresponding to column and row
                auto u = i / (float)ci;
                auto v = j / (float)cj;
                
                // compute new point location
                auto p = p00*u*v + p01*u*(1-v) + p10*(1-u)*v + p11*(1-u)*(1-v);
                
                // insert point into pos vector, remembering its index
                vertexidx[make_pair(i,j)] = mesh->pos.size();
                mesh->pos.push_back(p);
                mesh->norm.push_back(z3f);
            }
        }
        
        // foreach column
        for(auto i : range(ci)) {
            // foreach row
            for(auto j : range(cj)) {
                // find indices of neigboring vertices
                int idx0 = vertexidx[make_pair(i+0,j+0)];
                int idx1 = vertexidx[make_pair(i+1,j+0)];
                int idx2 = vertexidx[make_pair(i+1,j+1)];
                int idx3 = vertexidx[make_pair(i+0,j+1)];
                
                // create quad
                mesh->quad.push_back({idx0, idx1, idx2, idx3});
            }
        }
    } else {
        put_your_code_here("Implement sphere subdivision");
        
        // compute how much to subdivide
        int row = 1 << (surface->subdivision_level+1), column = row*2;

        mesh->pos.push_back(vec3f(surface->frame.o.x,surface->frame.o.y,surface->frame.o.z+radius));
        for(int i = 1 ; i < row; ++i)
            for(int j = 0 ; j < column ; ++j){
                double phi = 2*pi/column * j;
                double theta = pi/row * i;
                vec3f point(radius * cos(phi) * sin(theta) + surface->frame.o.x, radius * sin(phi) * sin(theta) + surface->frame.o.y, radius * cos(theta) + surface->frame.o.z);
                mesh->pos.push_back(point);
            }
        mesh->pos.push_back(vec3f(surface->frame.o.x,surface->frame.o.y,surface->frame.o.z-radius));


        // foreach column
            // foreach row
                // compute phi,theta for column and row
                // compute new point location
                // insert point into pos vector, remembering its index
          for(int i = 1; i <= column ; ++i)
             mesh->triangle.push_back(vec3i(0,i,i == column ? 1 : i+1));
          int last = 2 + (row-1)*column -1;
          for(int i = last - column; i < last  ; ++i){
              mesh->triangle.push_back(vec3i(i,last,i == last-1 ? last-column : i+1));
          }
          for(int i = 1; i < 2 + (row-1)*column - column -1 ; ++i)
            mesh->quad.push_back(vec4i(i, i+column, i%column == 0 ? i+1 : i+column+1, i%column == 0 ? i-column+1 : i+1));
        // foreach column
            // foreach row
                // find indices of neighboring vertices
                // create triangle (face touching pole) or quad
    }
    

    // according to smooth, either smooth_normals or facet_normals
    if(surface->subdivision_smooth) smooth_normals(mesh);
    else facet_normals(mesh);
    
    // update _display_mesh of surface
    surface->_display_mesh = mesh;
}

void subdivide(Scene* scene) {
    for(auto mesh : scene->meshes) {
        if(mesh->subdivision_catmullclark_level) subdivide_catmullclark(mesh);
        if(mesh->subdivision_bezier_level) subdivide_bezier(mesh);
    }
    for(auto surface : scene->surfaces) {
        subdivide_surface(surface);
    }
}


// main function
int main(int argc, char** argv) {    fout.open("data.txt");
    auto args = parse_cmdline(argc, argv,
        { "02_model", "view scene",
            {  {"resolution",     "r", "image resolution", typeid(int),    true,  jsonvalue() }  },
            {  {"scene_filename", "",  "scene filename",   typeid(string), false, jsonvalue("scene.json")},
               {"image_filename", "",  "image filename",   typeid(string), true,  jsonvalue("")}  }
        });
    
    // generate/load scene either by creating a test scene or loading from json file
    scene_filename = args.object_element("scene_filename").as_string();
    scene = nullptr;
    if(scene_filename.length() > 9 and scene_filename.substr(0,9) == "testscene") {
        int scene_type = atoi(scene_filename.substr(9).c_str());
        scene = create_test_scene(scene_type);
        scene_filename = scene_filename + ".json";
    } else {
        scene = load_json_scene(scene_filename);
    }
    error_if_not(scene, "scene is nullptr");
    
    image_filename = (args.object_element("image_filename").as_string() != "") ?
        args.object_element("image_filename").as_string() :
        scene_filename.substr(0,scene_filename.size()-5)+".png";
    
    if(not args.object_element("resolution").is_null()) {
        scene->image_height = args.object_element("resolution").as_int();
        scene->image_width = scene->camera->width * scene->image_height / scene->camera->height;
    }

    subdivide(scene);
    
    uiloop();
}




/////////////////////////////////////////////////////////////////////
// UI and Rendering Code: OpenGL, GLFW, GLSL


int gl_program_id         = 0;  // OpenGL program handle
int gl_vertex_shader_id   = 0;  // OpenGL vertex shader handle
int gl_fragment_shader_id = 0;  // OpenGL fragment shader handle
map<image3f*,int> gl_texture_id;// OpenGL texture handles

bool save      = false;         // whether to start the save loop
bool wireframe = false;         // display as wireframe

void init_shaders();            // initialize the shaders
void init_textures();           // initialize the textures
void shade();                   // render the scene with OpenGL
void _shade_mesh(Mesh* mesh);
void character_callback(GLFWwindow* window, unsigned int key);  // ...
                                // glfw callback for character input
void _bind_texture(string name_map, string name_on, image3f* txt, int pos); // ...
                                // utility to bind texture parameters for shaders
                                // uses texture name, texture_on name, texture pointer and texture unit position

// glfw callback for character input
void character_callback(GLFWwindow* window, unsigned int key) {
    if(key == 's') save = true;
    if(key == 'w') wireframe = not wireframe;
}

// uiloop
void uiloop() {
    auto ok_glfw = glfwInit();
    error_if_not(ok_glfw, "glfw init error");
    
    glfwWindowHint(GLFW_SAMPLES, scene->image_samples);
    
    auto window = glfwCreateWindow(scene->image_width,
                                   scene->image_height,
                                   "graphics13 | model", NULL, NULL);
    error_if_not(window, "glfw window error");
    
    glfwMakeContextCurrent(window);
    
    glfwSetCharCallback(window, character_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    
    auto ok_glew = glewInit();
    error_if_not(GLEW_OK == ok_glew, "glew init error");
    
    init_shaders();
    init_textures();
    
    auto mouse_last_x = -1.0;
    auto mouse_last_y = -1.0;
    
    while(not glfwWindowShouldClose(window)) {
        glfwGetFramebufferSize(window, &scene->image_width, &scene->image_height);
        scene->camera->width = (scene->camera->height * scene->image_width) / scene->image_height;
        
        shade();

        if(glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT)) {
            double x, y;
            glfwGetCursorPos(window, &x, &y);
            if (mouse_last_x < 0 or mouse_last_y < 0) { mouse_last_x = x; mouse_last_y = y; }
            auto delta_x = x - mouse_last_x, delta_y = y - mouse_last_y;
            
            set_view_turntable(scene->camera, delta_x*0.01, -delta_y*0.01, 0, 0, 0);
            
            mouse_last_x = x;
            mouse_last_y = y;
        } else { mouse_last_x = -1; mouse_last_y = -1; }
        
        if(save) {
            auto image = image3f(scene->image_width,scene->image_height);
            glReadPixels(0, 0, scene->image_width, scene->image_height, GL_RGB, GL_FLOAT, &image.at(0,0));
            write_png(image_filename, image, true);
            save = false;
        }
        
        glfwSwapBuffers(window);
        glfwPollEvents();
    }
    
    glfwDestroyWindow(window);
    
    glfwTerminate();
}

// initialize the shaders
void init_shaders() {
    // load shader code from files
    auto vertex_shader_code    = load_text_file("model_vertex.glsl");
    auto fragment_shader_code  = load_text_file("model_fragment.glsl");
    auto vertex_shader_codes   = (char *)vertex_shader_code.c_str();
    auto fragment_shader_codes = (char *)fragment_shader_code.c_str();

    // create shaders
    gl_vertex_shader_id   = glCreateShader(GL_VERTEX_SHADER);
    gl_fragment_shader_id = glCreateShader(GL_FRAGMENT_SHADER);
    
    // load shaders code onto the GPU
    glShaderSource(gl_vertex_shader_id,1,(const char**)&vertex_shader_codes,nullptr);
    glShaderSource(gl_fragment_shader_id,1,(const char**)&fragment_shader_codes,nullptr);
    
    // compile shaders
    glCompileShader(gl_vertex_shader_id);
    glCompileShader(gl_fragment_shader_id);
    
    // check if shaders are valid
    error_if_glerror();
    error_if_shader_not_valid(gl_vertex_shader_id);
    error_if_shader_not_valid(gl_fragment_shader_id);
    
    // create program
    gl_program_id = glCreateProgram();
    
    // attach shaders
    glAttachShader(gl_program_id,gl_vertex_shader_id);
    glAttachShader(gl_program_id,gl_fragment_shader_id);
    
    // bind vertex attributes locations
    glBindAttribLocation(gl_program_id, 0, "vertex_pos");
    glBindAttribLocation(gl_program_id, 1, "vertex_norm");
    glBindAttribLocation(gl_program_id, 2, "vertex_texcoord");

    // link program
    glLinkProgram(gl_program_id);
    
    // check if program is valid
    error_if_glerror();
    error_if_program_not_valid(gl_program_id);
}

// initialize the textures
void init_textures() {
    // grab textures from scene
    auto textures = get_textures(scene);
    // foreach texture
    for(auto texture : textures) {
        // if already in the gl_texture_id map, skip
        if(gl_texture_id.find(texture) != gl_texture_id.end()) continue;
        // gen texture id
        unsigned int id = 0;
        glGenTextures(1, &id);
        // set id to the gl_texture_id map for later use
        gl_texture_id[texture] = id;
        // bind texture
        glBindTexture(GL_TEXTURE_2D, id);
        // set texture filtering parameters
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_TRUE);
        // load texture data
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
                     texture->width(), texture->height(),
                     0, GL_RGB, GL_FLOAT, texture->data());
    }
}


// utility to bind texture parameters for shaders
// uses texture name, texture_on name, texture pointer and texture unit position
void _bind_texture(string name_map, string name_on, image3f* txt, int pos) {
    // if txt is not null
    if(txt) {
        // set texture on boolean parameter to true
        glUniform1i(glGetUniformLocation(gl_program_id,name_on.c_str()),GL_TRUE);
        // activate a texture unit at position pos
        glActiveTexture(GL_TEXTURE0+pos);
        // bind texture object to it from gl_texture_id map
        glBindTexture(GL_TEXTURE_2D, gl_texture_id[txt]);
        // set texture parameter to the position pos
        glUniform1i(glGetUniformLocation(gl_program_id, name_map.c_str()), pos);
    } else {
        // set texture on boolean parameter to false
        glUniform1i(glGetUniformLocation(gl_program_id,name_on.c_str()),GL_FALSE);
        // activate a texture unit at position pos
        glActiveTexture(GL_TEXTURE0+pos);
        // set zero as the texture id
        glBindTexture(GL_TEXTURE_2D, 0);
    }
}

// render the scene with OpenGL
void shade() {
    // enable depth test
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    // disable culling face
    glDisable(GL_CULL_FACE);
    // let the shader control the points
    glEnable(GL_POINT_SPRITE);
    
    // set up the viewport from the scene image size
    glViewport(0, 0, scene->image_width, scene->image_height);
    
    // clear the screen (both color and depth) - set cleared color to background
    glClearColor(scene->background.x, scene->background.y, scene->background.z, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    // enable program
    glUseProgram(gl_program_id);
    
    // bind camera's position, inverse of frame and projection
    // use frame_to_matrix_inverse and frustum_matrix
    glUniform3fv(glGetUniformLocation(gl_program_id,"camera_pos"),
                 1, &scene->camera->frame.o.x);
    glUniformMatrix4fv(glGetUniformLocation(gl_program_id,"camera_frame_inverse"),
                       1, true, &frame_to_matrix_inverse(scene->camera->frame)[0][0]);
    glUniformMatrix4fv(glGetUniformLocation(gl_program_id,"camera_projection"),
                       1, true, &frustum_matrix(-scene->camera->dist*scene->camera->width/2, scene->camera->dist*scene->camera->width/2,
                                                -scene->camera->dist*scene->camera->height/2, scene->camera->dist*scene->camera->height/2,
                                                scene->camera->dist,10000)[0][0]);
    
    // bind ambient and number of lights
    glUniform3fv(glGetUniformLocation(gl_program_id,"ambient"),1,&scene->ambient.x);
    glUniform1i(glGetUniformLocation(gl_program_id,"lights_num"),scene->lights.size());
    
    // foreach light
    auto count = 0;
    for(auto light : scene->lights) {
        // bind light position and internsity (create param name with tostring)
        glUniform3fv(glGetUniformLocation(gl_program_id,tostring("light_pos[%d]",count).c_str()),
                     1, &light->frame.o.x);
        glUniform3fv(glGetUniformLocation(gl_program_id,tostring("light_intensity[%d]",count).c_str()),
                     1, &light->intensity.x);
        count++;
    }
    
    // foreach mesh
    for(auto mesh : scene->meshes) {
        _shade_mesh(mesh);
    }
    
    for(auto surf : scene->surfaces) {
        _shade_mesh(surf->_display_mesh);
    }
}

void _shade_mesh(Mesh* mesh) {

    // bind material kd, ks, n
    ERROR_IF_NOT(mesh, "mesh is null");
    glUniform3fv(glGetUniformLocation(gl_program_id,"material_kd"),1,&mesh->mat->kd.x);
    glUniform3fv(glGetUniformLocation(gl_program_id,"material_ks"),1,&mesh->mat->ks.x);
    glUniform1f(glGetUniformLocation(gl_program_id,"material_n"),mesh->mat->n);
    
    // bind texture params (txt_on, sampler)
    _bind_texture("material_kd_txt",   "material_kd_txt_on",   mesh->mat->kd_txt,   0);
    _bind_texture("material_ks_txt",   "material_ks_txt_on",   mesh->mat->ks_txt,   1);
    _bind_texture("material_norm_txt", "material_norm_txt_on", mesh->mat->norm_txt, 2);
    
    // bind mesh frame - use frame_to_matrix
    glUniformMatrix4fv(glGetUniformLocation(gl_program_id,"mesh_frame"),1,true,&frame_to_matrix(mesh->frame)[0][0]);

    // enable vertex attributes arrays and set up pointers to the mesh data
    auto vertex_pos_location = glGetAttribLocation(gl_program_id, "vertex_pos");
    auto vertex_norm_location = glGetAttribLocation(gl_program_id, "vertex_norm");
    auto vertex_texcoord_location = glGetAttribLocation(gl_program_id, "vertex_texcoord");
    glEnableVertexAttribArray(vertex_pos_location);
    glVertexAttribPointer(vertex_pos_location, 3, GL_FLOAT, GL_FALSE, 0, &mesh->pos[0].x);
    glEnableVertexAttribArray(vertex_norm_location);
    glVertexAttribPointer(vertex_norm_location, 3, GL_FLOAT, GL_FALSE, 0, &mesh->norm[0].x);
    if(not mesh->texcoord.empty()) {
        glEnableVertexAttribArray(vertex_texcoord_location);
        glVertexAttribPointer(vertex_texcoord_location, 2, GL_FLOAT, GL_FALSE, 0, &mesh->texcoord[0].x);
    }
    else glVertexAttrib2f(vertex_texcoord_location, 0, 0);
    
    // draw triangles and quads
    if(not wireframe) {
        if(mesh->triangle.size()) glDrawElements(GL_TRIANGLES, mesh->triangle.size()*3, GL_UNSIGNED_INT, &mesh->triangle[0].x);
        if(mesh->quad.size()) glDrawElements(GL_QUADS, mesh->quad.size()*4, GL_UNSIGNED_INT, &mesh->quad[0].x);
    } else {
        auto edges = EdgeMap(mesh->triangle, mesh->quad).edges();
        glDrawElements(GL_LINES, edges.size()*2, GL_UNSIGNED_INT, &edges[0].x);
    }
    
    // draw line sets
    if(not mesh->line.empty()) glDrawElements(GL_LINES, mesh->line.size()*2, GL_UNSIGNED_INT, mesh->line.data());
    for(auto segment : mesh->spline) glDrawElements(GL_LINE_STRIP, 4, GL_UNSIGNED_INT, &segment);
    
    // disable vertex attribute arrays
    glDisableVertexAttribArray(vertex_pos_location);
    glDisableVertexAttribArray(vertex_norm_location);
    if(not mesh->texcoord.empty()) glDisableVertexAttribArray(vertex_texcoord_location);
}

