#include "W3DViewport.h"

#include "RenderObjUtils.h"

#include "agg_def.h"
#include "assetmgr.h"
#include "bmp2d.h"
#include "camera.h"
#include "dazzle.h"
#include "distlod.h"
#include "hanim.h"
#include "hlod.h"
#include "light.h"
#include "matrix3d.h"
#include "part_ldr.h"
#include "refcount.h"
#include "rendobj.h"
#include "mesh.h"
#include "meshmdl.h"
#include "ringobj.h"
#include "scene.h"
#include "soundrobj.h"
#include "sphereobj.h"
#include "vector2.h"
#include "vector3.h"
#include "ww3d.h"
#include "wwmath.h"

#include <QPaintEvent>
#include <QResizeEvent>
#include <QShowEvent>
#include <QMouseEvent>
#include <QDir>
#include <QWheelEvent>
#include <algorithm>
#include <cmath>

#ifdef _WIN32
#include <windows.h>
#endif

namespace {
constexpr double kPi = 3.14159265358979323846;
constexpr double kRadToDeg = 180.0 / kPi;
constexpr double kDegToRad = kPi / 180.0;
constexpr const char *kCameraBoneName = "CAMERA";

Matrix3D BuildCameraBoneTransform(const Matrix3D &bone_transform)
{
    Matrix3D cam_transform(Vector3(0.0f, -1.0f, 0.0f),
                           Vector3(0.0f, 0.0f, 1.0f),
                           Vector3(-1.0f, 0.0f, 0.0f),
                           Vector3(0.0f, 0.0f, 0.0f));
    return bone_transform * cam_transform;
}

bool GetCameraTransform(RenderObjClass *render_obj, Matrix3D &tm)
{
    if (!render_obj) {
        return false;
    }

    const int count = render_obj->Get_Num_Sub_Objects();
    for (int index = 0; index < count; ++index) {
        RenderObjClass *sub_obj = render_obj->Get_Sub_Object(index);
        if (!sub_obj) {
            continue;
        }

        const bool found = GetCameraTransform(sub_obj, tm);
        sub_obj->Release_Ref();
        if (found) {
            return true;
        }
    }

    const int bone_index = render_obj->Get_Bone_Index(kCameraBoneName);
    if (bone_index > 0) {
        tm = render_obj->Get_Bone_Transform(bone_index);
        return true;
    }

    return false;
}

void ToggleAlternateMaterials(RenderObjClass *render_obj)
{
    if (!render_obj) {
        return;
    }

    if (render_obj->Class_ID() == RenderObjClass::CLASSID_MESH) {
        auto *mesh = static_cast<MeshClass *>(render_obj);
        MeshModelClass *model = mesh->Get_Model();
        if (model) {
            model->Enable_Alternate_Material_Description(
                !model->Is_Alternate_Material_Description_Enabled());
        }
    }

    const int count = render_obj->Get_Num_Sub_Objects();
    for (int index = 0; index < count; ++index) {
        RenderObjClass *sub_obj = render_obj->Get_Sub_Object(index);
        ToggleAlternateMaterials(sub_obj);
    }
}

class W3DViewScene final : public SimpleSceneClass
{
public:
    void SetAllowLodSwitching(bool enabled) { _allowLodSwitching = enabled; }
    bool IsAllowLodSwitching() const { return _allowLodSwitching; }

    void Visibility_Check(CameraClass *camera) override
    {
        RefRenderObjListIterator it(&RenderList);
        for (it.First(); !it.Is_Done(); it.Next()) {
            RenderObjClass *robj = it.Peek_Obj();
            if (!robj) {
                continue;
            }

            if (robj->Is_Force_Visible()) {
                robj->Set_Visible(true);
            } else {
                robj->Set_Visible(!camera->Cull_Sphere(robj->Get_Bounding_Sphere()));
            }

            const int lod_level = robj->Get_LOD_Level();
            if (robj->Is_Really_Visible()) {
                robj->Prepare_LOD(*camera);
            }

            if (!_allowLodSwitching) {
                robj->Set_LOD_Level(lod_level);
            }
        }

        Visibility_Checked = true;
    }

    void Add_Render_Object(RenderObjClass *obj) override
    {
        SimpleSceneClass::Add_Render_Object(obj);
        Recalculate_Fog_Planes();
    }

    void Add_To_Lineup(RenderObjClass *obj)
    {
        if (!obj || !Can_Line_Up(obj)) {
            return;
        }

        AABoxClass obj_box;
        obj->Get_Obj_Space_Bounding_Box(obj_box);
        const float obj_width = obj_box.Extent.Y * 2.0f;

        const AABoxClass scene_box = Get_Line_Up_Bounding_Box();
        const float scene_width = scene_box.Extent.Y * 2.0f;

        const float new_scene_width = scene_width + obj_width + obj_width / 3.0f;
        const float delta = (new_scene_width - scene_width) / 2.0f;

        int existing_objects = 0;
        SceneIterator *it = Create_Iterator();
        if (it) {
            for (it->First(); !it->Is_Done(); it->Next()) {
                RenderObjClass *current = it->Current_Item();
                if (!current || !Can_Line_Up(current)) {
                    continue;
                }
                Vector3 pos = current->Get_Position();
                pos.Y -= delta;
                current->Set_Position(pos);
                ++existing_objects;
            }
            Destroy_Iterator(it);
        }

        if (existing_objects > 0) {
            obj->Set_Position(Vector3(0.0f, new_scene_width / 2.0f - obj_box.Extent.Y, 0.0f));
        } else {
            obj->Set_Position(Vector3(0.0f, 0.0f, 0.0f));
        }

        Add_Render_Object(obj);
        _lineupList.Add(obj);
    }

    void Clear_Lineup()
    {
        RenderObjClass *obj = nullptr;
        while ((obj = _lineupList.Remove_Head()) != nullptr) {
            Remove_Render_Object(obj);
        }
        Recalculate_Fog_Planes();
    }

    bool Can_Line_Up(RenderObjClass *obj) const
    {
        return obj && Can_Line_Up(obj->Class_ID());
    }

    bool Can_Line_Up(int class_id) const
    {
        return class_id == RenderObjClass::CLASSID_HMODEL ||
               class_id == RenderObjClass::CLASSID_HLOD;
    }

private:
    AABoxClass Get_Line_Up_Bounding_Box()
    {
        AABoxClass sum_of_boxes(Vector3(0.0f, 0.0f, 0.0f), Vector3(0.0f, 0.0f, 0.0f));
        SceneIterator *it = Create_Iterator();
        if (it) {
            for (it->First(); !it->Is_Done(); it->Next()) {
                RenderObjClass *current = it->Current_Item();
                if (current && Can_Line_Up(current)) {
                    sum_of_boxes.Add_Box(current->Get_Bounding_Box());
                }
            }
            Destroy_Iterator(it);
        }
        return sum_of_boxes;
    }

    SphereClass Get_Bounding_Sphere()
    {
        SphereClass bounding_sphere(Vector3(0.0f, 0.0f, 0.0f), 0.0f);
        SceneIterator *it = Create_Iterator();
        if (it) {
            for (it->First(); !it->Is_Done(); it->Next()) {
                RenderObjClass *current = it->Current_Item();
                if (!current) {
                    continue;
                }
                if (current->Class_ID() != RenderObjClass::CLASSID_LIGHT) {
                    bounding_sphere.Add_Sphere(current->Get_Bounding_Sphere());
                }
            }
            Destroy_Iterator(it);
        }
        return bounding_sphere;
    }

    void Recalculate_Fog_Planes()
    {
        const float kFogOpaqueMultiple = 8.0f;
        const float kFogMinimumDepth = 200.0f;
        float fog_near = 0.0f;
        float fog_far = 0.0f;
        Get_Fog_Range(&fog_near, &fog_far);

        const SphereClass sphere = Get_Bounding_Sphere();
        fog_far = sphere.Radius * kFogOpaqueMultiple;
        if (fog_far < fog_near + kFogMinimumDepth) {
            fog_far = fog_near + kFogMinimumDepth;
        }
        Set_Fog_Range(fog_near, fog_far);
    }

    bool _allowLodSwitching = false;
    RefRenderObjListClass _lineupList;
};

void SwitchLod(RenderObjClass *render_obj, int increment, bool &switched)
{
    if (!render_obj) {
        return;
    }

    const int count = render_obj->Get_Num_Sub_Objects();
    for (int index = 0; index < count; ++index) {
        RenderObjClass *sub_obj = render_obj->Get_Sub_Object(index);
        if (sub_obj) {
            SwitchLod(sub_obj, increment, switched);
            sub_obj->Release_Ref();
        }
    }

    if (render_obj->Class_ID() == RenderObjClass::CLASSID_HLOD) {
        auto *hlod = static_cast<HLodClass *>(render_obj);
        hlod->Set_LOD_Level(hlod->Get_LOD_Level() + increment);
        switched = true;
    }
}
} // namespace

static void SetLowestLod(RenderObjClass *render_obj);
static void ResetSceneLod(SceneClass *scene);

W3DViewport::W3DViewport(QWidget *parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_NativeWindow);
    setAttribute(Qt::WA_PaintOnScreen);
    setAttribute(Qt::WA_NoSystemBackground);
    setAttribute(Qt::WA_OpaquePaintEvent);
    setAutoFillBackground(false);
    setFocusPolicy(Qt::StrongFocus);

    _timer.setInterval(16);
    connect(&_timer, &QTimer::timeout, this, &W3DViewport::renderFrame);
}

W3DViewport::~W3DViewport()
{
    clearAnimation();
    shutdownWW3D();
}

QPaintEngine *W3DViewport::paintEngine() const
{
    return nullptr;
}

void W3DViewport::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    initWW3D();

    if (_initialized && !_timer.isActive()) {
        _elapsed.restart();
        _timer.start();
    }
}

void W3DViewport::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    if (_initialized && _windowed) {
        WW3D::Set_Device_Resolution(width(), height(), _bitsPerPixel, 1, true);
        updateCameraFov(width(), height());
    }
}

void W3DViewport::hideEvent(QHideEvent *event)
{
    QWidget::hideEvent(event);
    if (_timer.isActive()) {
        _timer.stop();
    }
}

void W3DViewport::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
}

void W3DViewport::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        _leftDown = true;
    } else if (event->button() == Qt::RightButton) {
        _rightDown = true;
    }

    _lastPos = event->pos();
    grabMouse();
    event->accept();
}

void W3DViewport::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        _leftDown = false;
    } else if (event->button() == Qt::RightButton) {
        _rightDown = false;
    }

    if (!_leftDown && !_rightDown) {
        releaseMouse();
    }

    event->accept();
}

void W3DViewport::mouseMoveEvent(QMouseEvent *event)
{
    if (!_initialized || !_camera) {
        _lastPos = event->pos();
        return;
    }

    const QPoint current = event->pos();
    const int delta_x = _lastPos.x() - current.x();
    const int delta_y = _lastPos.y() - current.y();

    if (_leftDown && _rightDown) {
        const float mid_x = width() * 0.5f;
        const float mid_y = height() * 0.5f;
        if (mid_x > 0.0f && mid_y > 0.0f) {
            const float last_x = (static_cast<float>(_lastPos.x()) - mid_x) / mid_x;
            const float last_y = (mid_y - static_cast<float>(_lastPos.y())) / mid_y;
            const float point_x = (static_cast<float>(current.x()) - mid_x) / mid_x;
            const float point_y = (mid_y - static_cast<float>(current.y())) / mid_y;

            const Vector3 camera_pan(-1.0f * _cameraDistance * (point_x - last_x),
                                     -1.0f * _cameraDistance * (point_y - last_y),
                                     0.0f);

            Matrix3D transform = _camera->Get_Transform();
            transform.Translate(camera_pan);

            const Matrix3 view = Build_Matrix3(_rotation);
            const Vector3 move = view * camera_pan;
            _orbitCenter += move;

            _camera->Set_Transform(transform);
        }
    } else if (_leftDown) {
        const float mid_x = width() * 0.5f;
        const float mid_y = height() * 0.5f;
        if (mid_x > 0.0f && mid_y > 0.0f) {
            const float last_x = (static_cast<float>(_lastPos.x()) - mid_x) / mid_x;
            const float last_y = (mid_y - static_cast<float>(_lastPos.y())) / mid_y;
            const float point_x = (static_cast<float>(current.x()) - mid_x) / mid_x;
            const float point_y = (mid_y - static_cast<float>(current.y())) / mid_y;

            Quaternion rotation = Trackball(last_x, last_y, point_x, point_y, 0.8f);

            if (_allowedRotation == CameraRotation::OnlyX) {
                Matrix3D temp_matrix = Build_Matrix3D(rotation);
                Matrix3D temp_matrix2(1);
                temp_matrix2.Rotate_X(temp_matrix.Get_X_Rotation());
                temp_matrix2.Set_Translation(temp_matrix.Get_Translation());
                rotation = Build_Quaternion(temp_matrix2);
            } else if (_allowedRotation == CameraRotation::OnlyY) {
                Matrix3D temp_matrix = Build_Matrix3D(rotation);
                Matrix3D temp_matrix2(1);
                temp_matrix2.Rotate_Y(temp_matrix.Get_Y_Rotation());
                temp_matrix2.Set_Translation(temp_matrix.Get_Translation());
                rotation = Build_Quaternion(temp_matrix2);
            } else if (_allowedRotation == CameraRotation::OnlyZ) {
                Matrix3D temp_matrix = Build_Matrix3D(rotation);
                Matrix3D temp_matrix2(1);
                temp_matrix2.Rotate_Z(temp_matrix.Get_Z_Rotation());
                temp_matrix2.Set_Translation(temp_matrix.Get_Translation());
                rotation = Build_Quaternion(temp_matrix2);
            }

            _rotation = rotation;

            Matrix3D transform = _camera->Get_Transform();
            Matrix3D inverse;
            transform.Get_Orthogonal_Inverse(inverse);

            const Vector3 to_object = inverse * _orbitCenter;
            transform.Translate(to_object);
            Matrix3D::Multiply(transform, Build_Matrix3D(rotation), &transform);
            transform.Translate(-to_object);

            _camera->Set_Transform(transform);
        }
    } else if (_rightDown) {
        if (height() > 0 && delta_y != 0) {
            Matrix3D transform = _camera->Get_Transform();
            const float delta = static_cast<float>(delta_y) / static_cast<float>(height());
            float adjustment = delta * _cameraDistance * 3.0f;

            if ((adjustment < _minZoomAdjust) && (adjustment >= 0.0f)) {
                adjustment = _minZoomAdjust;
            }
            if ((adjustment > -_minZoomAdjust) && (adjustment <= 0.0f)) {
                adjustment = -_minZoomAdjust;
            }

            if ((_cameraDistance + adjustment) > 0.0f) {
                _cameraDistance += adjustment;
                transform.Translate(Vector3(0.0f, 0.0f, adjustment));
                _camera->Set_Transform(transform);
            }
        }
    }

    _lastPos = current;
    event->accept();
}

void W3DViewport::wheelEvent(QWheelEvent *event)
{
    if (!_initialized || !_camera) {
        return;
    }

    const int delta = event->angleDelta().y();
    if (delta == 0) {
        return;
    }

    Matrix3D transform = _camera->Get_Transform();
    const float steps = static_cast<float>(delta) / 120.0f;
    float adjustment = -steps * _cameraDistance * 0.15f;

    if ((adjustment < _minZoomAdjust) && (adjustment >= 0.0f)) {
        adjustment = _minZoomAdjust;
    }
    if ((adjustment > -_minZoomAdjust) && (adjustment <= 0.0f)) {
        adjustment = -_minZoomAdjust;
    }

    if ((_cameraDistance + adjustment) > 0.0f) {
        _cameraDistance += adjustment;
        transform.Translate(Vector3(0.0f, 0.0f, adjustment));
        _camera->Set_Transform(transform);
    }

    event->accept();
}

void W3DViewport::renderFrame()
{
    if (!_initialized) {
        return;
    }

    const qint64 elapsed_ms = _elapsed.restart();
    updateFrameTiming(static_cast<float>(elapsed_ms));
    if (elapsed_ms > 0) {
        const auto next_time = WW3D::Get_Sync_Time() + static_cast<unsigned int>(elapsed_ms);
        WW3D::Sync(next_time);
        updateAnimation(static_cast<float>(elapsed_ms) / 1000.0f);
    }

    updateCameraAnimation();
    updateObjectRotation();
    updateLightRotation();
    renderScene();
}

void W3DViewport::renderScene()
{
    if (_allowLodSwitching && _scene) {
        ResetSceneLod(_scene);
    }

    WW3D::Begin_Render(true, true, _clearColor);
    if (_backgroundScene && _backgroundCamera) {
        WW3D::Render(_backgroundScene, _backgroundCamera, false, false);
    }
    if (_backgroundObjectScene && _backgroundObjectCamera) {
        if (_camera) {
            Matrix3D transform = _camera->Get_Transform();
            _backgroundObjectCamera->Set_Transform(transform);
            _backgroundObjectCamera->Set_Position(Vector3(0.0f, 0.0f, 0.0f));
        }
        WW3D::Render(_backgroundObjectScene, _backgroundObjectCamera, false, false);
    }
    if (_scene && _camera) {
        WW3D::Render(_scene, _camera, false, false);
        if (_dazzleLayer) {
            _dazzleLayer->Render(_camera);
        }
    }
    WW3D::End_Render(true);
}

void W3DViewport::renderFrameWithTicks(int ticks)
{
    if (!_initialized) {
        return;
    }

    if (ticks > 0) {
        updateFrameTiming(static_cast<float>(ticks));
        const auto next_time = WW3D::Get_Sync_Time() + static_cast<unsigned int>(ticks);
        WW3D::Sync(next_time);
        updateAnimation(static_cast<float>(ticks) / 1000.0f);
    }

    updateCameraAnimation();
    updateObjectRotation();
    updateLightRotation();
    renderScene();
}

void W3DViewport::updateFrameTiming(float elapsedMs)
{
    if (elapsedMs <= 0.0f) {
        return;
    }

    _frameTimeAccumMs += elapsedMs;
    ++_frameTimeSamples;

    if (_frameTimeAccumMs >= 1000.0f) {
        _averageFrameMs = _frameTimeAccumMs / static_cast<float>(_frameTimeSamples);
        _frameTimeAccumMs = 0.0f;
        _frameTimeSamples = 0;
    }
}

static void SetLowestLod(RenderObjClass *render_obj)
{
    if (!render_obj) {
        return;
    }

    const int count = render_obj->Get_Num_Sub_Objects();
    for (int index = 0; index < count; ++index) {
        RenderObjClass *sub_obj = render_obj->Get_Sub_Object(index);
        if (sub_obj) {
            SetLowestLod(sub_obj);
            sub_obj->Release_Ref();
        }
    }

    if (render_obj->Class_ID() == RenderObjClass::CLASSID_HLOD) {
        static_cast<HLodClass *>(render_obj)->Set_LOD_Level(0);
    }
}

static void ResetSceneLod(SceneClass *scene)
{
    if (!scene) {
        return;
    }

    SceneIterator *it = scene->Create_Iterator();
    if (!it) {
        return;
    }

    for (it->First(); !it->Is_Done(); it->Next()) {
        RenderObjClass *obj = it->Current_Item();
        SetLowestLod(obj);
    }

    scene->Destroy_Iterator(it);
}

void W3DViewport::initWW3D()
{
    if (_initialized) {
        return;
    }

    createWinId();
    void *hwnd = reinterpret_cast<void *>(winId());

    if (WW3D::Init(hwnd, nullptr, false) != WW3D_ERROR_OK) {
        return;
    }

    WW3D::Enable_Static_Sort_Lists(true);

    if (WW3D::Set_Render_Device(-1, width(), height(), 32, 1, true) != WW3D_ERROR_OK) {
        WW3D::Shutdown();
        return;
    }
    _windowed = true;
    _bitsPerPixel = 32;

    if (auto *asset_manager = WW3DAssetManager::Get_Instance()) {
        asset_manager->Register_Prototype_Loader(&_HLodLoader);
        asset_manager->Register_Prototype_Loader(&_DistLODLoader);
        asset_manager->Register_Prototype_Loader(&_ParticleEmitterLoader);
        asset_manager->Register_Prototype_Loader(&_AggregateLoader);
        asset_manager->Register_Prototype_Loader(&_RingLoader);
        asset_manager->Register_Prototype_Loader(&_SphereLoader);
        asset_manager->Register_Prototype_Loader(&_SoundRenderObjLoader);
        asset_manager->Load_Procedural_Textures();
    }

    if (DazzleRenderObjClass::Get_Type_Class(0)) {
        _dazzleLayer = new DazzleLayerClass();
        DazzleRenderObjClass::Set_Current_Dazzle_Layer(_dazzleLayer);
        DazzleRenderObjClass::Enable_Dazzle_Rendering(true);
    } else {
        DazzleRenderObjClass::Set_Current_Dazzle_Layer(nullptr);
        DazzleRenderObjClass::Enable_Dazzle_Rendering(false);
    }

    initScene();
    _initialized = true;
}

void W3DViewport::shutdownWW3D()
{
    if (!_initialized) {
        return;
    }

    _timer.stop();
    shutdownScene();
    if (_dazzleLayer) {
        DazzleRenderObjClass::Set_Current_Dazzle_Layer(nullptr);
        delete _dazzleLayer;
        _dazzleLayer = nullptr;
    }
    DazzleRenderObjClass::Enable_Dazzle_Rendering(false);
    WW3D::Shutdown();
    _initialized = false;
}

void W3DViewport::initScene()
{
    if (_scene || _camera) {
        return;
    }

    _scene = NEW_REF(W3DViewScene, ());
    setWireframeEnabled(_wireframeEnabled);
    _scene->Set_Ambient_Light(_ambientLight);
    _scene->Set_Fog_Color(_clearColor);
    _scene->Set_Fog_Enable(_fogEnabled);
    setLodAutoSwitchingEnabled(_allowLodSwitching);

    _backgroundScene = NEW_REF(SimpleSceneClass, ());
    _backgroundCamera = NEW_REF(CameraClass, ());
    _backgroundCamera->Set_View_Plane(Vector2(-1.0f, -1.0f), Vector2(1.0f, 1.0f));
    _backgroundCamera->Set_Position(Vector3(0.0f, 0.0f, 1.0f));
    _backgroundCamera->Set_Clip_Planes(0.1f, 10.0f);
    refreshBackgroundBitmap();

    _backgroundObjectScene = NEW_REF(SimpleSceneClass, ());
    _backgroundObjectScene->Set_Ambient_Light(Vector3(0.5f, 0.5f, 0.5f));
    _backgroundObjectCamera = NEW_REF(CameraClass, ());
    _backgroundObjectCamera->Set_View_Plane(Vector2(-1.0f, -1.0f), Vector2(1.0f, 1.0f));
    _backgroundObjectCamera->Set_Position(Vector3(0.0f, 0.0f, 0.0f));
    _backgroundObjectCamera->Set_Clip_Planes(0.1f, 10.0f);
    if (!_backgroundObjectName.isEmpty()) {
        setBackgroundObjectName(_backgroundObjectName);
    }

    _sceneLight = NEW_REF(LightClass, ());
    _sceneLight->Set_Position(Vector3(0.0f, 5000.0f, 3000.0f));
    _sceneLight->Set_Intensity(1.0f);
    _sceneLight->Set_Force_Visible(true);
    _sceneLight->Set_Flag(LightClass::NEAR_ATTENUATION, false);
    _sceneLight->Set_Far_Attenuation_Range(1000000.0, 1000000.0);
    _sceneLight->Set_Ambient(Vector3(0.0f, 0.0f, 0.0f));
    _sceneLight->Set_Diffuse(Vector3(1.0f, 1.0f, 1.0f));
    _sceneLight->Set_Specular(Vector3(1.0f, 1.0f, 1.0f));
    _scene->Add_Render_Object(_sceneLight);

    _camera = NEW_REF(CameraClass, ());
    Matrix3D transform(1);
    transform.Look_At(Vector3(35.0f, 0.0f, 0.0f), Vector3(0.0f, 0.0f, 0.0f), 0);
    _camera->Set_Transform(transform);
    _camera->Set_Clip_Planes(0.2f, 10000.0f);
    updateCameraFov(width(), height());
    if (_manualClipPlanes) {
        _camera->Set_Clip_Planes(_manualNear, _manualFar);
    }

    if (_renderObject) {
        _scene->Add_Render_Object(_renderObject);
        resetCameraToObject(*_renderObject);
    } else {
        auto *placeholder = NEW_REF(SphereRenderObjClass, ());
        placeholder->Set_Color(Vector3(0.8f, 0.2f, 0.2f));
        placeholder->Set_Scale(Vector3(10.0f, 10.0f, 10.0f));
        setRenderObject(placeholder);
        placeholder->Release_Ref();
    }

    applySceneLightSettings();
}

void W3DViewport::shutdownScene()
{
    if (_scene) {
        auto *viewer_scene = static_cast<W3DViewScene *>(_scene);
        viewer_scene->Clear_Lineup();
    }
    if (_scene && _renderObject) {
        _scene->Remove_Render_Object(_renderObject);
    }
    if (_scene && _sceneLight) {
        _scene->Remove_Render_Object(_sceneLight);
    }
    if (_backgroundBitmapObj) {
        _backgroundBitmapObj->Remove();
    }
    REF_PTR_RELEASE(_backgroundBitmapObj);
    if (_backgroundObject) {
        _backgroundObject->Remove();
    }
    REF_PTR_RELEASE(_backgroundObject);
    REF_PTR_RELEASE(_backgroundObjectCamera);
    REF_PTR_RELEASE(_backgroundObjectScene);
    REF_PTR_RELEASE(_backgroundCamera);
    REF_PTR_RELEASE(_backgroundScene);
    REF_PTR_RELEASE(_animation);
    REF_PTR_RELEASE(_renderObject);
    REF_PTR_RELEASE(_sceneLight);
    REF_PTR_RELEASE(_camera);
    REF_PTR_RELEASE(_scene);
}

void W3DViewport::updateCameraFov(int width, int height, bool force)
{
    if (!_camera || width <= 0 || height <= 0) {
        return;
    }

    if (_manualFov && !force) {
        if (_manualHfov > 0.0 && _manualVfov > 0.0) {
            _camera->Set_View_Plane(_manualHfov, _manualVfov);
        }
        return;
    }

    float hfov = DEG_TO_RADF(45.0f);
    float vfov = DEG_TO_RADF(45.0f);

    if (height > width) {
        vfov = DEG_TO_RADF(45.0f);
        hfov = (static_cast<float>(width) / static_cast<float>(height)) * vfov;
    } else {
        hfov = DEG_TO_RADF(45.0f);
        vfov = (static_cast<float>(height) / static_cast<float>(width)) * hfov;
    }

    _camera->Set_View_Plane(hfov, vfov);
}

void W3DViewport::resetCameraToObject(RenderObjClass &object)
{
    if (!_camera) {
        return;
    }

    const SphereClass sphere = object.Get_Bounding_Sphere();
    const Vector3 old_center = _orbitCenter;
    _orbitCenter = sphere.Center;
    _cameraDistance = std::max(1.0f, sphere.Radius * 3.0f);
    _minZoomAdjust = _cameraDistance / 190.0f;
    Matrix3D transform(1);
    transform.Look_At(_orbitCenter + Vector3(_cameraDistance, 0.0f, 0.0f), _orbitCenter, 0);
    _rotation = Build_Quaternion(transform);
    _camera->Set_Transform(transform);
    if (!_manualClipPlanes) {
        const float min_clip = std::max(0.2f, _minZoomAdjust * 0.5f);
        _camera->Set_Clip_Planes(min_clip, _cameraDistance * 60.0f);
        if (_scene) {
            float fog_start = 0.0f;
            float fog_end = 0.0f;
            _scene->Get_Fog_Range(&fog_start, &fog_end);
            _scene->Set_Fog_Range(min_clip, fog_end);
        }
    }

    const int bone_index = object.Get_Bone_Index(kCameraBoneName);
    if (bone_index > 0) {
        Matrix3D bone_transform = object.Get_Bone_Transform(bone_index);
        if (_cameraBonePosX) {
            bone_transform = BuildCameraBoneTransform(bone_transform);
        }
        _camera->Set_Transform(bone_transform);
    }

    if (_sceneLight) {
        if (!_sceneLightOrientationSet && !_sceneLightDistanceSet) {
            Matrix3D light_tm(1);
            light_tm.Set_Translation(_orbitCenter);
            light_tm.Translate(Vector3(0.0f, 0.0f, 0.7f * _cameraDistance));
            _sceneLight->Set_Transform(light_tm);
            _sceneLightDistance = 0.7f * _cameraDistance;
        } else {
            updateSceneLightPosition(old_center);
        }
    }
}

void W3DViewport::setRenderObject(RenderObjClass *object)
{
    if (_scene && _renderObject) {
        _scene->Remove_Render_Object(_renderObject);
    }
    if (_scene) {
        auto *viewer_scene = static_cast<W3DViewScene *>(_scene);
        viewer_scene->Clear_Lineup();
    }
    REF_PTR_RELEASE(_renderObject);

    if (!object) {
        return;
    }

    REF_PTR_SET(_renderObject, object);
    if (_scene) {
        _scene->Add_Render_Object(_renderObject);
        if (_autoResetCamera) {
            resetCameraToObject(*_renderObject);
        }
    }

    if (_renderObject) {
        if (_animationCombo) {
            _renderObject->Set_Animation(_animationCombo);
        } else if (_animation) {
            if (_animationBlend) {
                _renderObject->Set_Animation(_animation, _animationFrame);
            } else {
                _renderObject->Set_Animation(_animation, static_cast<int>(_animationFrame));
            }
        }
    }
}

void W3DViewport::setManualFovEnabled(bool enabled)
{
    _manualFov = enabled;
    if (!_camera) {
        return;
    }

    if (_manualFov) {
        if (_manualHfov > 0.0 && _manualVfov > 0.0) {
            _camera->Set_View_Plane(_manualHfov, _manualVfov);
        }
    } else {
        updateCameraFov(width(), height(), true);
    }
}

bool W3DViewport::isManualFovEnabled() const
{
    return _manualFov;
}

void W3DViewport::setManualClipPlanesEnabled(bool enabled)
{
    _manualClipPlanes = enabled;
    if (_manualClipPlanes && _camera) {
        _camera->Set_Clip_Planes(_manualNear, _manualFar);
    }
}

bool W3DViewport::isManualClipPlanesEnabled() const
{
    return _manualClipPlanes;
}

void W3DViewport::setCameraFovDegrees(double hfov_deg, double vfov_deg)
{
    const double hfov = hfov_deg * kDegToRad;
    const double vfov = vfov_deg * kDegToRad;
    _manualHfov = hfov;
    _manualVfov = vfov;
    if (_camera) {
        _camera->Set_View_Plane(hfov, vfov);
    }
}

void W3DViewport::cameraFovDegrees(double &hfov_deg, double &vfov_deg) const
{
    double hfov = _manualHfov;
    double vfov = _manualVfov;
    if (_camera) {
        hfov = _camera->Get_Horizontal_FOV();
        vfov = _camera->Get_Vertical_FOV();
    } else if (hfov <= 0.0 || vfov <= 0.0) {
        hfov = 45.0 * kDegToRad;
        vfov = 45.0 * kDegToRad;
    }

    hfov_deg = hfov * kRadToDeg;
    vfov_deg = vfov * kRadToDeg;
}

void W3DViewport::setCameraClipPlanes(float znear, float zfar)
{
    _manualNear = znear;
    _manualFar = zfar;
    if (_camera) {
        _camera->Set_Clip_Planes(znear, zfar);
    }
    if (_scene) {
        float fog_start = 0.0f;
        float fog_end = 0.0f;
        _scene->Get_Fog_Range(&fog_start, &fog_end);
        _scene->Set_Fog_Range(znear, fog_end);
    }
}

void W3DViewport::cameraClipPlanes(float &znear, float &zfar) const
{
    if (_camera) {
        _camera->Get_Clip_Planes(znear, zfar);
        return;
    }

    znear = _manualNear;
    zfar = _manualFar;
}

void W3DViewport::resetFov()
{
    updateCameraFov(width(), height(), true);
}

void W3DViewport::setCameraDistance(float distance)
{
    if (!_camera) {
        _cameraDistance = distance;
        return;
    }

    _cameraDistance = distance;
    if (_cameraDistance < 0.0f) {
        _cameraDistance = 0.0f;
    }

    Matrix3D transform(1);
    transform.Look_At(_orbitCenter + Vector3(_cameraDistance, 0.0f, 0.0f), _orbitCenter, 0.0f);
    _camera->Set_Transform(transform);
    _rotation = Build_Quaternion(transform);
    _minZoomAdjust = _cameraDistance / 190.0f;
}

float W3DViewport::cameraDistance() const
{
    return _cameraDistance;
}

bool W3DViewport::applyResolution(int width, int height, int bitsPerPixel, bool fullscreen)
{
    if (!_initialized) {
        return false;
    }

    const int bpp = bitsPerPixel > 0 ? bitsPerPixel : _bitsPerPixel;
    const int windowed = fullscreen ? 0 : 1;
    const int resx = fullscreen ? width : this->width();
    const int resy = fullscreen ? height : this->height();

    const WW3DErrorType result =
        WW3D::Set_Render_Device(-1, resx, resy, bpp, windowed, false);
    if (result != WW3D_ERROR_OK) {
        return false;
    }

    _bitsPerPixel = bpp;
    _windowed = !fullscreen;
    updateCameraFov(resx, resy, true);
    if (_manualClipPlanes) {
        setCameraClipPlanes(_manualNear, _manualFar);
    }

    return true;
}

bool W3DViewport::addToLineup(RenderObjClass *object)
{
    if (!_scene || !object) {
        return false;
    }

    auto *viewer_scene = static_cast<W3DViewScene *>(_scene);
    if (!viewer_scene->Can_Line_Up(object)) {
        return false;
    }

    viewer_scene->Add_To_Lineup(object);
    return true;
}

bool W3DViewport::canLineUpClass(int class_id) const
{
    if (!_scene) {
        return false;
    }

    auto *viewer_scene = static_cast<W3DViewScene *>(_scene);
    return viewer_scene->Can_Line_Up(class_id);
}

void W3DViewport::clearLineup()
{
    if (!_scene) {
        return;
    }

    auto *viewer_scene = static_cast<W3DViewScene *>(_scene);
    viewer_scene->Clear_Lineup();
}

void W3DViewport::setObjectRotationFlags(int flags)
{
    _objectRotation = flags;
}

int W3DViewport::objectRotationFlags() const
{
    return _objectRotation;
}

void W3DViewport::resetObjectTransform()
{
    if (!_renderObject) {
        return;
    }

    _renderObject->Set_Transform(Matrix3D(1));
}

void W3DViewport::toggleAlternateMaterials()
{
    ToggleAlternateMaterials(_renderObject);
}

void W3DViewport::setLightRotationFlags(int flags)
{
    _lightRotation = flags;
}

int W3DViewport::lightRotationFlags() const
{
    return _lightRotation;
}

float W3DViewport::currentScreenSize() const
{
    if (!_renderObject || !_camera) {
        return 0.0f;
    }

    return _renderObject->Get_Screen_Size(*_camera);
}

void W3DViewport::setCameraPosition(CameraPosition position)
{
    if (!_camera || !_renderObject) {
        return;
    }

    const SphereClass sphere = _renderObject->Get_Bounding_Sphere();
    const Vector3 old_center = _orbitCenter;
    _orbitCenter = sphere.Center;
    _cameraDistance = sphere.Radius * 3.0f;
    if (_cameraDistance < 1.0f) {
        _cameraDistance = 1.0f;
    }
    if (_cameraDistance > 400.0f) {
        _cameraDistance = 400.0f;
    }

    _minZoomAdjust = _cameraDistance / 190.0f;

    Matrix3D transform(1);
    switch (position) {
    case CameraPosition::Front:
        transform.Look_At(_orbitCenter + Vector3(_cameraDistance, 0.0f, 0.0f), _orbitCenter, 0.0f);
        break;
    case CameraPosition::Back:
        transform.Look_At(_orbitCenter + Vector3(-_cameraDistance, 0.0f, 0.0f), _orbitCenter, 0.0f);
        break;
    case CameraPosition::Left:
        transform.Look_At(_orbitCenter + Vector3(0.0f, -_cameraDistance, 0.0f), _orbitCenter, 0.0f);
        break;
    case CameraPosition::Right:
        transform.Look_At(_orbitCenter + Vector3(0.0f, _cameraDistance, 0.0f), _orbitCenter, 0.0f);
        break;
    case CameraPosition::Top:
        transform.Look_At(_orbitCenter + Vector3(0.0f, 0.0f, _cameraDistance), _orbitCenter, 3.1415926535f);
        break;
    case CameraPosition::Bottom:
        transform.Look_At(_orbitCenter + Vector3(0.0f, 0.0f, -_cameraDistance), _orbitCenter, 3.1415926535f);
        break;
    }

    _camera->Set_Transform(transform);
    _rotation = Build_Quaternion(transform);
    updateSceneLightPosition(old_center);
}

void W3DViewport::resetCamera()
{
    if (_renderObject) {
        resetCameraToObject(*_renderObject);
    }
}

void W3DViewport::setAllowedCameraRotation(CameraRotation rotation)
{
    _allowedRotation = rotation;
}

W3DViewport::CameraRotation W3DViewport::allowedCameraRotation() const
{
    return _allowedRotation;
}

void W3DViewport::setAutoResetEnabled(bool enabled)
{
    _autoResetCamera = enabled;
}

bool W3DViewport::isAutoResetEnabled() const
{
    return _autoResetCamera;
}

void W3DViewport::setCameraAnimationEnabled(bool enabled)
{
    _animateCamera = enabled;
}

bool W3DViewport::isCameraAnimationEnabled() const
{
    return _animateCamera;
}

void W3DViewport::setCameraBonePosX(bool enabled)
{
    _cameraBonePosX = enabled;
}

bool W3DViewport::isCameraBonePosX() const
{
    return _cameraBonePosX;
}

void W3DViewport::setAnimation(HAnimClass *animation)
{
    if (_animationCombo) {
        delete _animationCombo;
        _animationCombo = nullptr;
    }
    REF_PTR_RELEASE(_animation);
    REF_PTR_SET(_animation, animation);
    _animationTime = 0.0f;
    _animationFrame = 0.0f;
    _animationState = AnimationState::Playing;

    if (_renderObject && _animation) {
        _renderObject->Set_Animation(_animation, 0);
    }
}

void W3DViewport::setAnimationCombo(HAnimComboClass *combo)
{
    if (_animationCombo) {
        delete _animationCombo;
    }

    _animationCombo = combo;
    REF_PTR_RELEASE(_animation);
    _animationTime = 0.0f;
    _animationFrame = 0.0f;
    _animationState = AnimationState::Playing;

    if (_animationCombo) {
        _animation = _animationCombo->Get_Motion(0);
    }

    if (_renderObject && _animationCombo) {
        _renderObject->Set_Animation(_animationCombo);
    }
}

void W3DViewport::clearAnimation()
{
    _animationTime = 0.0f;
    _animationFrame = 0.0f;
    _animationState = AnimationState::Stopped;
    REF_PTR_RELEASE(_animation);
    if (_animationCombo) {
        delete _animationCombo;
        _animationCombo = nullptr;
    }
    if (_renderObject) {
        _renderObject->Set_Animation();
    }
}

bool W3DViewport::animationStatus(int &currentFrame, int &totalFrames, float &fps) const
{
    if (!_animation) {
        currentFrame = 0;
        totalFrames = 0;
        fps = 0.0f;
        return false;
    }

    totalFrames = _animation->Get_Num_Frames();
    currentFrame = static_cast<int>(_animationFrame);
    const float frame_rate = _animation->Get_Frame_Rate();
    fps = frame_rate * _animationSpeed;
    return totalFrames > 0;
}

float W3DViewport::averageFrameMilliseconds() const
{
    return _averageFrameMs;
}

void W3DViewport::setBackgroundColor(const Vector3 &color)
{
    _clearColor = color;
    if (_scene) {
        _scene->Set_Fog_Color(color);
    }
}

Vector3 W3DViewport::backgroundColor() const
{
    return _clearColor;
}

void W3DViewport::setBackgroundBitmap(const QString &path)
{
    _backgroundBitmap = path;
    refreshBackgroundBitmap();
}

QString W3DViewport::backgroundBitmap() const
{
    return _backgroundBitmap;
}

void W3DViewport::setBackgroundObjectName(const QString &name)
{
    _backgroundObjectName = name;

    if (_backgroundObject && _backgroundObjectScene) {
        _backgroundObject->Remove();
    }
    REF_PTR_RELEASE(_backgroundObject);

    if (name.trimmed().isEmpty() || !_backgroundObjectScene) {
        return;
    }

    auto *asset_manager = WW3DAssetManager::Get_Instance();
    if (!asset_manager) {
        return;
    }

    const QByteArray native = name.toLatin1();
    _backgroundObject = asset_manager->Create_Render_Obj(native.constData());
    if (!_backgroundObject) {
        return;
    }

    _backgroundObject->Set_Position(Vector3(0.0f, 0.0f, 0.0f));
    const float camera_depth = _backgroundObject->Get_Bounding_Sphere().Radius * 4.0f;
    if (_backgroundObjectCamera && _camera) {
        _backgroundObjectCamera->Set_Transform(_camera->Get_Transform());
        _backgroundObjectCamera->Set_Position(Vector3(0.0f, 0.0f, 0.0f));
        _backgroundObjectCamera->Set_Clip_Planes(1.0f, camera_depth);
    }

    _backgroundObjectScene->Add_Render_Object(_backgroundObject);
}

QString W3DViewport::backgroundObjectName() const
{
    return _backgroundObjectName;
}

void W3DViewport::setAmbientLight(const Vector3 &color)
{
    _ambientLight = color;
    if (_scene) {
        _scene->Set_Ambient_Light(color);
    }
}

Vector3 W3DViewport::ambientLight() const
{
    return _ambientLight;
}

void W3DViewport::setFogEnabled(bool enabled)
{
    _fogEnabled = enabled;
    if (_scene) {
        _scene->Set_Fog_Enable(enabled);
    }
}

bool W3DViewport::isFogEnabled() const
{
    return _fogEnabled;
}

void W3DViewport::setSceneLightColor(const Vector3 &color)
{
    _sceneLightColor = color;
    if (_sceneLight) {
        _sceneLight->Set_Diffuse(color);
        _sceneLight->Set_Specular(color);
    }
}

Vector3 W3DViewport::sceneLightColor() const
{
    if (_sceneLight) {
        Vector3 color;
        _sceneLight->Get_Diffuse(&color);
        return color;
    }

    return _sceneLightColor;
}

void W3DViewport::setSceneLightOrientation(const Quaternion &orientation)
{
    _sceneLightOrientation = orientation;
    _sceneLightOrientationSet = true;
    if (!_sceneLight) {
        return;
    }

    float distance = sceneLightDistance();
    if (distance <= 0.0f) {
        distance = std::max(1.0f, _cameraDistance);
    }

    Matrix3D light_tm(1);
    light_tm.Set_Translation(_orbitCenter);
    Matrix3D::Multiply(light_tm, Build_Matrix3D(orientation), &light_tm);
    light_tm.Translate(Vector3(0.0f, 0.0f, distance));
    _sceneLight->Set_Transform(light_tm);
}

Quaternion W3DViewport::sceneLightOrientation() const
{
    if (_sceneLight) {
        return Build_Quaternion(_sceneLight->Get_Transform());
    }

    return _sceneLightOrientation;
}

void W3DViewport::setSceneLightDistance(float distance)
{
    _sceneLightDistance = distance;
    _sceneLightDistanceSet = true;
    if (!_sceneLight) {
        return;
    }

    Vector3 direction = _sceneLight->Get_Position() - _orbitCenter;
    float length = direction.Length();
    if (length <= 0.0f) {
        direction = Vector3(0.0f, 0.0f, 1.0f);
    } else {
        direction.Normalize();
    }

    _sceneLight->Set_Position(_orbitCenter + direction * distance);
}

float W3DViewport::sceneLightDistance() const
{
    if (_sceneLight) {
        return (_sceneLight->Get_Position() - _orbitCenter).Length();
    }

    return _sceneLightDistance;
}

void W3DViewport::setSceneLightIntensity(float intensity)
{
    _sceneLightIntensity = intensity;
    if (_sceneLight) {
        _sceneLight->Set_Intensity(intensity);
    }
}

float W3DViewport::sceneLightIntensity() const
{
    if (_sceneLight) {
        return _sceneLight->Get_Intensity();
    }

    return _sceneLightIntensity;
}

void W3DViewport::setSceneLightAttenuation(float start, float end, bool enabled)
{
    _sceneLightAttenStart = start;
    _sceneLightAttenEnd = end;
    _sceneLightAttenEnabled = enabled;
    if (_sceneLight) {
        _sceneLight->Set_Far_Attenuation_Range(start, end);
        _sceneLight->Set_Flag(LightClass::FAR_ATTENUATION, enabled);
    }
}

void W3DViewport::sceneLightAttenuation(float &start, float &end, bool &enabled) const
{
    if (_sceneLight) {
        double start_d = 0.0;
        double end_d = 0.0;
        _sceneLight->Get_Far_Attenuation_Range(start_d, end_d);
        start = static_cast<float>(start_d);
        end = static_cast<float>(end_d);
        enabled = _sceneLight->Get_Flag(LightClass::FAR_ATTENUATION) != 0;
        return;
    }

    start = _sceneLightAttenStart;
    end = _sceneLightAttenEnd;
    enabled = _sceneLightAttenEnabled;
}

void W3DViewport::setWireframeEnabled(bool enabled)
{
    _wireframeEnabled = enabled;
    if (_scene) {
        _scene->Set_Polygon_Mode(enabled ? SceneClass::LINE : SceneClass::FILL);
    }
}

bool W3DViewport::isWireframeEnabled() const
{
    return _wireframeEnabled;
}

void W3DViewport::setLodAutoSwitchingEnabled(bool enabled)
{
    _allowLodSwitching = enabled;
    auto *scene = static_cast<W3DViewScene *>(_scene);
    if (scene) {
        scene->SetAllowLodSwitching(enabled);
    }
}

bool W3DViewport::isLodAutoSwitchingEnabled() const
{
    if (auto *scene = static_cast<W3DViewScene *>(_scene)) {
        return scene->IsAllowLodSwitching();
    }

    return _allowLodSwitching;
}

bool W3DViewport::currentLodInfo(int &level, int &count) const
{
    if (!_renderObject || _renderObject->Class_ID() != RenderObjClass::CLASSID_HLOD) {
        return false;
    }

    auto *hlod = static_cast<HLodClass *>(_renderObject);
    level = hlod->Get_LOD_Level();
    count = hlod->Get_LOD_Count();
    return true;
}

bool W3DViewport::setNullLodIncluded(bool enabled)
{
    if (!_renderObject || _renderObject->Class_ID() != RenderObjClass::CLASSID_HLOD) {
        return false;
    }

    auto *hlod = static_cast<HLodClass *>(_renderObject);
    hlod->Include_NULL_Lod(enabled);
    UpdateLodPrototype(*hlod);
    return true;
}

bool W3DViewport::isNullLodIncluded() const
{
    if (!_renderObject || _renderObject->Class_ID() != RenderObjClass::CLASSID_HLOD) {
        return false;
    }

    auto *hlod = static_cast<HLodClass *>(_renderObject);
    return hlod->Is_NULL_Lod_Included();
}

bool W3DViewport::recordLodScreenArea()
{
    if (!_renderObject || !_camera ||
        _renderObject->Class_ID() != RenderObjClass::CLASSID_HLOD) {
        return false;
    }

    auto *hlod = static_cast<HLodClass *>(_renderObject);
    const float screen_size = _renderObject->Get_Screen_Size(*_camera);
    hlod->Set_Max_Screen_Size(hlod->Get_LOD_Level(), screen_size);
    UpdateLodPrototype(*hlod);
    return true;
}

bool W3DViewport::adjustLodLevel(int delta)
{
    bool switched = false;
    SwitchLod(_renderObject, delta, switched);
    return switched;
}

void W3DViewport::applySceneLightSettings()
{
    if (!_sceneLight) {
        return;
    }

    setSceneLightColor(_sceneLightColor);
    setSceneLightIntensity(_sceneLightIntensity);
    setSceneLightAttenuation(_sceneLightAttenStart, _sceneLightAttenEnd, _sceneLightAttenEnabled);

    if (_sceneLightOrientationSet) {
        setSceneLightOrientation(_sceneLightOrientation);
    }

    if (_sceneLightDistanceSet) {
        setSceneLightDistance(_sceneLightDistance);
    }
}

void W3DViewport::updateSceneLightPosition(const Vector3 &oldCenter)
{
    if (!_sceneLight) {
        return;
    }

    Vector3 direction = _sceneLight->Get_Position() - oldCenter;
    float distance = direction.Length();
    if (distance <= 0.0f) {
        direction = Vector3(0.0f, 0.0f, 1.0f);
        distance = std::max(1.0f, _cameraDistance);
    } else {
        direction.Normalize();
    }

    _sceneLight->Set_Position(_orbitCenter + direction * distance);
    _sceneLightDistance = distance;
}

void W3DViewport::refreshBackgroundBitmap()
{
    if (!_backgroundScene) {
        return;
    }

    if (_backgroundBitmapObj) {
        _backgroundBitmapObj->Remove();
        _backgroundBitmapObj->Release_Ref();
        _backgroundBitmapObj = nullptr;
    }

    if (_backgroundBitmap.trimmed().isEmpty()) {
        return;
    }

    const QByteArray native = QDir::toNativeSeparators(_backgroundBitmap).toLocal8Bit();
    _backgroundBitmapObj = NEW_REF(Bitmap2DObjClass, (native.constData(), 0.5f, 0.5f, true, false));
    if (_backgroundBitmapObj) {
        _backgroundScene->Add_Render_Object(_backgroundBitmapObj);
    }
}

void W3DViewport::updateAnimation(float deltaSeconds)
{
    if (!_animation || !_renderObject) {
        return;
    }

    if (_animationState != AnimationState::Playing) {
        return;
    }

    const int total_frames = _animation->Get_Num_Frames();
    const float frame_rate = _animation->Get_Frame_Rate();
    if (total_frames <= 1 || frame_rate <= 0.0f) {
        _renderObject->Set_Animation(_animation, 0);
        return;
    }

    const float loop_time = static_cast<float>(total_frames - 1) / frame_rate;
    _animationTime += deltaSeconds * _animationSpeed;
    if (_animationTime > loop_time) {
        _animationTime = std::fmod(_animationTime, loop_time);
    }

    _animationFrame = frame_rate * _animationTime;
    if (_animationCombo) {
        const int count = _animationCombo->Get_Num_Anims();
        for (int index = 0; index < count; ++index) {
            _animationCombo->Set_Frame(index, _animationFrame);
        }
        _renderObject->Set_Animation(_animationCombo);
    } else {
        if (_animationBlend) {
            _renderObject->Set_Animation(_animation, _animationFrame);
        } else {
            _renderObject->Set_Animation(_animation, static_cast<int>(_animationFrame));
        }
    }
}

void W3DViewport::setAnimationState(AnimationState state)
{
    if (_animationState == state) {
        return;
    }

    _animationState = state;

    if (!_renderObject || !_animation) {
        return;
    }

    if (state == AnimationState::Stopped) {
        _animationTime = 0.0f;
        _animationFrame = 0.0f;
        if (_animationCombo) {
            const int count = _animationCombo->Get_Num_Anims();
            for (int index = 0; index < count; ++index) {
                _animationCombo->Set_Frame(index, 0.0f);
            }
            _renderObject->Set_Animation(_animationCombo);
        } else {
            _renderObject->Set_Animation(_animation, 0);
        }
    }
}

W3DViewport::AnimationState W3DViewport::animationState() const
{
    return _animationState;
}

void W3DViewport::setAnimationSpeed(float speed)
{
    if (speed <= 0.0f) {
        speed = 0.01f;
    }
    _animationSpeed = speed;
}

float W3DViewport::animationSpeed() const
{
    return _animationSpeed;
}

void W3DViewport::setAnimationBlend(bool enabled)
{
    if (_animationBlend == enabled) {
        return;
    }

    _animationBlend = enabled;
    if (!_renderObject || !_animation || _animationCombo) {
        return;
    }

    if (_animationBlend) {
        _renderObject->Set_Animation(_animation, _animationFrame);
    } else {
        _renderObject->Set_Animation(_animation, static_cast<int>(_animationFrame));
    }
}

bool W3DViewport::animationBlend() const
{
    return _animationBlend;
}

bool W3DViewport::stepAnimation(int delta)
{
    if (!_animation || !_renderObject) {
        return false;
    }

    const int total_frames = _animation->Get_Num_Frames();
    if (total_frames <= 1) {
        if (_animationCombo) {
            const int count = _animationCombo->Get_Num_Anims();
            for (int index = 0; index < count; ++index) {
                _animationCombo->Set_Frame(index, 0.0f);
            }
            _renderObject->Set_Animation(_animationCombo);
        } else {
            _renderObject->Set_Animation(_animation, 0);
        }
        _animationFrame = 0.0f;
        _animationTime = 0.0f;
        return true;
    }

    int frame = static_cast<int>(_animationFrame) + delta;
    if (frame >= total_frames) {
        frame = 0;
    } else if (frame < 0) {
        frame = total_frames - 1;
    }

    _animationFrame = static_cast<float>(frame);
    const float frame_rate = _animation->Get_Frame_Rate();
    if (frame_rate > 0.0f) {
        _animationTime = _animationFrame / frame_rate;
    } else {
        _animationTime = 0.0f;
    }
    if (_animationCombo) {
        const int count = _animationCombo->Get_Num_Anims();
        for (int index = 0; index < count; ++index) {
            _animationCombo->Set_Frame(index, _animationFrame);
        }
        _renderObject->Set_Animation(_animationCombo);
    } else {
        _renderObject->Set_Animation(_animation, frame);
    }
    return true;
}

void W3DViewport::applyAnimationFrame(float frame)
{
    _animationFrame = frame;
    if (!_animation || !_renderObject) {
        return;
    }

    const float frame_rate = _animation->Get_Frame_Rate();
    if (frame_rate > 0.0f) {
        _animationTime = _animationFrame / frame_rate;
    } else {
        _animationTime = 0.0f;
    }

    if (_animationCombo) {
        const int count = _animationCombo->Get_Num_Anims();
        for (int index = 0; index < count; ++index) {
            _animationCombo->Set_Frame(index, _animationFrame);
        }
        _renderObject->Set_Animation(_animationCombo);
    } else {
        _renderObject->Set_Animation(_animation, _animationFrame);
    }
}

bool W3DViewport::hasAnimation() const
{
    return _animation != nullptr;
}

bool W3DViewport::captureMovie(const QString &baseName, float frameRate, QString *error)
{
    if (!_renderObject || !_animation) {
        if (error) {
            *error = "No animation is available for capture.";
        }
        return false;
    }

    if (frameRate <= 0.0f) {
        frameRate = 30.0f;
    }

    const int total_frames = _animation->Get_Num_Frames();
    const float anim_rate = _animation->Get_Frame_Rate();
    if (total_frames <= 1 || anim_rate <= 0.0f) {
        if (error) {
            *error = "Animation has no frames to capture.";
        }
        return false;
    }

    const bool timer_active = _timer.isActive();
    if (timer_active) {
        _timer.stop();
    }

    const AnimationState prev_state = _animationState;
    const float prev_frame = _animationFrame;

    setAnimationState(AnimationState::Paused);
    applyAnimationFrame(0.0f);

    const QByteArray base_bytes = baseName.trimmed().isEmpty()
        ? QByteArray("Grab")
        : baseName.toLatin1();

    WW3D::Pause_Movie(true);
    WW3D::Start_Movie_Capture(base_bytes.constData(), frameRate);
    WW3D::Pause_Movie(true);

    const float frame_inc = anim_rate / frameRate;
    const int ticks = static_cast<int>(1000.0f / frameRate);

    for (float frame = 0.0f; frame <= (static_cast<float>(total_frames) - 1.0f); frame += frame_inc) {
        applyAnimationFrame(frame);
        renderFrameWithTicks(ticks);
        WW3D::Update_Movie_Capture();
#ifdef _WIN32
        if (::GetAsyncKeyState(VK_ESCAPE) < 0) {
            break;
        }
#endif
    }

    WW3D::Stop_Movie_Capture();

    if (prev_state == AnimationState::Stopped) {
        setAnimationState(AnimationState::Stopped);
    } else {
        applyAnimationFrame(prev_frame);
        _animationState = prev_state;
    }

    if (timer_active) {
        _elapsed.restart();
        _timer.start();
    }

    return true;
}

bool W3DViewport::toggleSubobjectLod()
{
    if (!_renderObject) {
        return false;
    }

    const bool enabled = _renderObject->Is_Sub_Objects_Match_LOD_Enabled() != 0;
    _renderObject->Set_Sub_Objects_Match_LOD(!enabled);
    UpdateAggregatePrototype(*_renderObject);
    return !enabled;
}

bool W3DViewport::isSubobjectLodBound() const
{
    if (!_renderObject) {
        return false;
    }

    return _renderObject->Is_Sub_Objects_Match_LOD_Enabled() != 0;
}

void W3DViewport::updateCameraAnimation()
{
    if (!_animateCamera || !_renderObject || !_camera) {
        return;
    }

    Matrix3D bone_transform(1);
    if (!GetCameraTransform(_renderObject, bone_transform)) {
        return;
    }

    const Matrix3D camera_transform = BuildCameraBoneTransform(bone_transform);
    _camera->Set_Transform(camera_transform);
}

void W3DViewport::updateObjectRotation()
{
    if (!_renderObject || _objectRotation == RotateNone) {
        return;
    }

    Matrix3D transform = _renderObject->Get_Transform();

    if (_objectRotation & RotateX) {
        transform.Rotate_X(0.05f);
    } else if (_objectRotation & RotateXBack) {
        transform.Rotate_X(-0.05f);
    }
    if (_objectRotation & RotateY) {
        transform.Rotate_Y(-0.05f);
    } else if (_objectRotation & RotateYBack) {
        transform.Rotate_Y(0.05f);
    }
    if (_objectRotation & RotateZ) {
        transform.Rotate_Z(0.05f);
    } else if (_objectRotation & RotateZBack) {
        transform.Rotate_Z(-0.05f);
    }

    if (!transform.Is_Orthogonal()) {
        transform.Re_Orthogonalize();
    }

    _renderObject->Set_Transform(transform);
}

void W3DViewport::updateLightRotation()
{
    if (!_sceneLight || !_renderObject || _lightRotation == RotateNone) {
        return;
    }

    Matrix3D rotation_matrix(1);
    if (_lightRotation & RotateX) {
        rotation_matrix.Rotate_X(0.05f);
    } else if (_lightRotation & RotateXBack) {
        rotation_matrix.Rotate_X(-0.05f);
    }
    if (_lightRotation & RotateY) {
        rotation_matrix.Rotate_Y(-0.05f);
    } else if (_lightRotation & RotateYBack) {
        rotation_matrix.Rotate_Y(0.05f);
    }
    if (_lightRotation & RotateZ) {
        rotation_matrix.Rotate_Z(0.05f);
    } else if (_lightRotation & RotateZBack) {
        rotation_matrix.Rotate_Z(-0.05f);
    }

    Matrix3D coord_inv;
    Matrix3D coord_to_obj;
    Matrix3D coord_system = _renderObject->Get_Transform();
    coord_system.Get_Orthogonal_Inverse(coord_inv);

    Matrix3D transform = _sceneLight->Get_Transform();
    Matrix3D::Multiply(coord_inv, transform, &coord_to_obj);
    Matrix3D::Multiply(coord_system, rotation_matrix, &transform);
    Matrix3D::Multiply(transform, coord_to_obj, &transform);

    if (!transform.Is_Orthogonal()) {
        transform.Re_Orthogonalize();
    }

    _sceneLight->Set_Transform(transform);
}
