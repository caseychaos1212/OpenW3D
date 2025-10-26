/*
**	Command & Conquer Renegade(tm)
**	Copyright 2025 Electronic Arts Inc.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
**
**	This program is distributed in the hope that it will be useful,
**	but WITHOUT ANY WARRANTY; without even the implied warranty of
**	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**	GNU General Public License for more details.
**
**	You should have received a copy of the GNU General Public License
**	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

/***********************************************************************************************
 ***              C O N F I D E N T I A L  ---  W E S T W O O D  S T U D I O S               ***
 ***********************************************************************************************
 *                                                                                             *
 *                 Project Name : WWPhys                                                       *
 *                                                                                             *
 *                     $Archive:: /Commando/Code/wwphys/phys.cpp                              $*
 *                                                                                             *
 *                       Author:: Greg Hjelstrom                                               *
 *                                                                                             *
 *                     $Modtime:: 2/21/02 4:01p                                               $*
 *                                                                                             *
 *                    $Revision:: 79                                                          $*
 *                                                                                             *
 *---------------------------------------------------------------------------------------------*
 * Functions:                                                                                  *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */


#include "phys.h"
#include "colmathaabox.h"
#include "rendobj.h"
#include "rinfo.h"
#include "assetmgr.h"
#include "ww3d.h"
#include "wwdebug.h"
#include <string>
#include "pscene.h"
#include "cullsys.h"
#include "chunkio.h"
#include "saveload.h"
#include "persistfactory.h"
#include "physcoltest.h"
#include "lightenvironment.h"
#include "umbrasupport.h"
#if (UMBRASUPPORT)
#include <umbra.hpp>
#endif

#include "dx8wrapper.h"
#include "dx8vertexbuffer.h"
#include "dx8indexbuffer.h"


const float DEBUG_RENDER_DIST2 = (50.0f*50.0f);
const float SUN_CHECK_DISTANCE = 50.0f;	//If a ray this long doesn't intersect, you can see the sun...


/*
** create_render_obj_from_filename
*/
RenderObjClass * create_render_obj_from_filename( const char * filename )
{
	StringClass	render_obj_name(filename,true);
	if ( ::strchr( filename, '\\' ) != 0 ) {
		render_obj_name = ::strrchr( filename, '\\' ) + 1;
	}
	render_obj_name.Erase( render_obj_name.Get_Length() - 4, 4 );

	RenderObjClass *model = WW3DAssetManager::Get_Instance()->Create_Render_Obj( render_obj_name );
	if ( model == NULL ) {
		WWDEBUG_SAY(("Failed to create %s from %s\n", (const char *)render_obj_name, filename));
	}
	return model;
}


/***********************************************************************************************
**
** PhysClass Implementation
**
***********************************************************************************************/

/*
** Chunk ID's used by PhysClass
*/
enum
{
	PHYS_CHUNK_VARIABLES				= 0x00660055,
	PHYS_CHUNK_MODEL,
	PHYS_CHUNK_SIMPLE_SHAPE,

	PHYS_VARIABLE_CULLABLE_PTR		= 0x00,
	PHYS_VARIABLE_WIDGETUSER_PTR,
	PHYS_VARIABLE_EDITABLE_PTR,
	PHYS_VARIABLE_FLAGS,
	PHYS_VARIABLE_NAME,
	PHYS_VARIABLE_OBSERVER,
	PHYS_VARIABLE_DEFID,
	PHYS_VARIABLE_INSTANCEID,
	PHYS_SIMPLE_SHAPE_TYPE,
	PHYS_SIMPLE_SHAPE_BOX_EXTENTS,
	PHYS_SIMPLE_SHAPE_SPHERE_RADIUS,
};




PhysClass::PhysClass(void) :
	Flags(DEFAULT_FLAGS),
	Model(NULL),
	Observer(NULL),
	Definition(NULL),
	InstanceID(0),
	VisObjectID(0),
	LastVisibleFrame(0),	// JANI TEMP TEST
	SunStatusLastUpdated(0),
	StaticLightingCache(NULL)
#if (UMBRASUPPORT)
	,UmbraObject(NULL)
#endif
{
#if (UMBRASUPPORT)
	UmbraObject = new Umbra::Object(UmbraSupport::Peek_Dummy_Sphere());
	UmbraObject->setUserPointer(this);
	UmbraSupport::Install_Umbra_Object(this);
#endif
	SimpleShape = SimpleShapeDefinition();
	FallbackTransform.Make_Identity();
}

PhysClass::~PhysClass(void)
{
	if (Model) {
		Model->Release_Ref();
		Model = NULL;
	}
	if (StaticLightingCache) {
		delete StaticLightingCache;
	}
#if (UMBRASUPPORT)
	if (UmbraObject) {
		UmbraSupport::Remove_Umbra_Object(this);
		UmbraObject->release();
		UmbraObject = NULL;
	}
#endif
}

void PhysClass::Init(const PhysDefClass & def)
{
	Definition = &def;
	Flags = DEFAULT_FLAGS; 
	if (!def.ModelName.Is_Empty()) {

		RenderObjClass * model = NULL;
	
		if (::strchr(def.ModelName, '.') != NULL) {
			model = ::create_render_obj_from_filename(def.ModelName);
		} else {
			model = WW3DAssetManager::Get_Instance()->Create_Render_Obj(def.ModelName);
		}

		if ( model == NULL ) {
			WWDEBUG_SAY(( "***  FATAL ERROR : Failed to create model %s\n", def.ModelName ));
		}

		Set_Model(model);
		REF_PTR_RELEASE(model);
	}

	const SimpleShapeDefinition &shape_def = def.Get_Simple_Shape_Definition();
	SimpleShape = shape_def;
	switch (shape_def.Type) {
	case PHYS_SIMPLE_SHAPE_AA_BOX:
		Set_Simple_Shape_AABox(shape_def.BoxHalfExtents);
		break;
	case PHYS_SIMPLE_SHAPE_SPHERE:
		Set_Simple_Shape_Sphere(shape_def.SphereRadius);
		break;
	default:
		SimpleShape = SimpleShapeDefinition();
		Update_Cull_Box();
		break;
	}
}

void PhysClass::Set_Model(RenderObjClass * model)	
{ 
	PhysicsWorldClass * world = PhysicsWorldClass::Get_Active_World();
	bool in_scene = (world != NULL) ? world->Contains(this) : false;
	PhysicsSceneClass * scene = dynamic_cast<PhysicsSceneClass *>(world);
	Matrix3D previous_transform(true);

	if (Model) {
		previous_transform = Model->Get_Transform();
		// If we had an old model, copy the transform
		if ( model ) {	
			model->Set_Transform( Model->Get_Transform() );
		}
		if (in_scene && scene != NULL) {
			Model->Notify_Removed(scene);
		}
		Model->Release_Ref();
	} else if (model != NULL) {
		model->Set_Transform(FallbackTransform);
	}
	Model = model; 
	if (Model) {
		Model->Add_Ref(); 
		if (in_scene && scene != NULL) {
			Model->Notify_Added(scene);
		}
		FallbackTransform = Model->Get_Transform();
	} else {
		FallbackTransform = previous_transform;
	}

	if ((Definition != NULL) && (Definition->IsPreLit)) {
		Enable_Is_Pre_Lit(true);
	}
	Invalidate_Static_Lighting_Cache ();
	Update_Cull_Box();
}
	
void PhysClass::Set_Model_By_Name(const char * model_type_name)
{
	RenderObjClass * model = WW3DAssetManager::Get_Instance()->Create_Render_Obj(model_type_name);
	if ( model == NULL ) {
		WWDEBUG_SAY(( "%s failed to load\n", model_type_name ));
	}
	WWASSERT(model);		// As above, PhysClasses cannot survive without a model...

	Set_Model(model);
	if (model) {
		model->Release_Ref();
	}
}

const Matrix3D & PhysClass::Get_Transform(void) const
{
	if (Model) {
		return Model->Get_Transform();
	}
	return FallbackTransform;
}

void PhysClass::Set_Transform(const Matrix3D & m)
{
	if (Model) {
		Model->Set_Transform(m);
	}
	FallbackTransform = m;
	Update_Cull_Box();
	Invalidate_Static_Lighting_Cache();
}

void PhysClass::Set_Simple_Shape_AABox(const Vector3 &half_extents)
{
	Vector3 clamped;
	clamped.X = WWMath::Max(half_extents.X, 0.0f);
	clamped.Y = WWMath::Max(half_extents.Y, 0.0f);
	clamped.Z = WWMath::Max(half_extents.Z, 0.0f);
	SimpleShape.Type = PHYS_SIMPLE_SHAPE_AA_BOX;
	SimpleShape.BoxHalfExtents = clamped;
	SimpleShape.SphereRadius = WWMath::Max(clamped.X, WWMath::Max(clamped.Y, clamped.Z));
	Update_Simple_Shape_Cull_Box();
}

void PhysClass::Set_Simple_Shape_Sphere(float radius)
{
	float positive_radius = WWMath::Max(radius, 0.0f);
	SimpleShape.Type = PHYS_SIMPLE_SHAPE_SPHERE;
	SimpleShape.SphereRadius = positive_radius;
	SimpleShape.BoxHalfExtents.Set(positive_radius, positive_radius, positive_radius);
	Update_Simple_Shape_Cull_Box();
}

void PhysClass::Update_Simple_Shape_Cull_Box(void)
{
	if (!Has_Simple_Shape()) {
		return;
	}

	AABoxClass box;
	Vector3 center;
	const Matrix3D &tm = Get_Transform();
	tm.Get_Translation(&center);
	box.Center = center;

	if (SimpleShape.Type == PHYS_SIMPLE_SHAPE_AA_BOX) {
		Vector3 x_axis;
		Vector3 y_axis;
		Vector3 z_axis;
		tm.Get_X_Vector(&x_axis);
		tm.Get_Y_Vector(&y_axis);
		tm.Get_Z_Vector(&z_axis);
		x_axis *= SimpleShape.BoxHalfExtents.X;
		y_axis *= SimpleShape.BoxHalfExtents.Y;
		z_axis *= SimpleShape.BoxHalfExtents.Z;
		Vector3 extent;
		extent.X = WWMath::Fabs(x_axis.X) + WWMath::Fabs(y_axis.X) + WWMath::Fabs(z_axis.X);
		extent.Y = WWMath::Fabs(x_axis.Y) + WWMath::Fabs(y_axis.Y) + WWMath::Fabs(z_axis.Y);
		extent.Z = WWMath::Fabs(x_axis.Z) + WWMath::Fabs(y_axis.Z) + WWMath::Fabs(z_axis.Z);
		box.Extent = extent;
	} else if (SimpleShape.Type == PHYS_SIMPLE_SHAPE_SPHERE) {
		float radius = WWMath::Fabs(SimpleShape.SphereRadius);
		box.Extent.Set(radius, radius, radius);
	}

	Set_Cull_Box(box);
}

RenderObjClass * PhysClass::Get_Model(void)				
{ 
	if (Model) Model->Add_Ref(); 
	return Model; 
}

void PhysClass::Set_Name(const char * name)
{
	Name=name;
}

const char * PhysClass::Get_Name(void)
{
	return Name;
}

void PhysClass::Get_Shadow_Blob_Box(AABoxClass * set_obj_space_box)
{
	WWASSERT(set_obj_space_box != NULL);
	if (set_obj_space_box != NULL) {
		if (Model) {
			Model->Get_Obj_Space_Bounding_Box(*set_obj_space_box);
			set_obj_space_box->Extent *= 0.75f;
		} else if (Has_Simple_Shape()) {
			set_obj_space_box->Center.Set(0.0f,0.0f,0.0f);
			if (SimpleShape.Type == PHYS_SIMPLE_SHAPE_AA_BOX) {
				Vector3 extents = SimpleShape.BoxHalfExtents;
				extents *= 0.75f;
				set_obj_space_box->Extent = extents;
			} else if (SimpleShape.Type == PHYS_SIMPLE_SHAPE_SPHERE) {
				float radius = SimpleShape.SphereRadius * 0.75f;
				set_obj_space_box->Extent.Set(radius, radius, radius);
			}
		} else {
			set_obj_space_box->Center.Set(0.0f,0.0f,0.0f);
			set_obj_space_box->Extent.Set(0.0f,0.0f,0.0f);
		}
	}
}

void PhysClass::Render(RenderInfoClass & rinfo)
{ 
	Push_Effects(rinfo);

	if (Model) { 
		Model->Render(rinfo); 
	} 

	Pop_Effects(rinfo);
}

void PhysClass::Vis_Render(SpecialRenderInfoClass & rinfo)
{
	if (Model) {
		Model->Special_Render(rinfo);
	}
}

void PhysClass::Invalidate_Static_Lighting_Cache(void)
{
	Set_Flag(STATIC_LIGHTING_DIRTY,true);
}

LightEnvironmentClass * PhysClass::Get_Static_Lighting_Environment(void)
{
	if (Is_Pre_Lit()) {
	
		/*
		** This object doesn't need a lighting cache, make sure it doesn't have one
		*/
		if (StaticLightingCache != NULL) {
			WWDEBUG_SAY(("Pre-Lit object %s has a lighting cache!\r\n",Model->Get_Name()));
			delete StaticLightingCache;
			StaticLightingCache = NULL;
		}
	
	} else if (Get_Flag(STATIC_LIGHTING_DIRTY)) {

		/*
		** First, ensure that we have an allocated lighting cache
		*/
		if (StaticLightingCache == NULL) {
			StaticLightingCache = new LightEnvironmentClass;
		}

		/*
		** Next, update our sunlight status
		*/
		Update_Sun_Status();

		/*
		** Finally, ask the physics scene to re-compute our lighting cache
		*/
		if (PhysicsWorldClass * world = PhysicsWorldClass::Get_Active_World()) {
			world->Compute_Static_Lighting(StaticLightingCache,
														Model->Get_Bounding_Sphere().Center,
														Get_Flag(IS_IN_THE_SUN),
														Get_Vis_Object_ID());
			Set_Flag(STATIC_LIGHTING_DIRTY,false);
		}
	} 

	/*
	** Return our lighting cache to the caller
	*/
	return StaticLightingCache;
}

void PhysClass::Update_Sun_Status(void)
{
	// Update sun status only four times per second
	unsigned current_time=WW3D::Get_Sync_Time();
	if ((current_time-SunStatusLastUpdated)<250) return;
	SunStatusLastUpdated=current_time;

	PhysicsWorldClass * world = PhysicsWorldClass::Get_Active_World();
	if (world == NULL) {
		return;
	}
	
	Vector3 sunlight;
	world->Get_Sun_Light_Vector(&sunlight);
	Vector3 center = Model->Get_Bounding_Sphere().Center; 

// FIXME (gth) Need a collision group for sun-rays
	CastResultStruct sunresult;
	LineSegClass sunray(center,center - sunlight * SUN_CHECK_DISTANCE);
	PhysRayCollisionTestClass sunraytest(sunray,&sunresult,0,COLLISION_TYPE_PROJECTILE);
	sunraytest.CheckStaticObjs = true;
	sunraytest.CheckDynamicObjs = false;

	Inc_Ignore_Counter();
	world->Cast_Ray(sunraytest);

	// if the ray hits a static object which is casting a projected shadow, ignore that object 
	// and check again.
	if (	(sunresult.Fraction < 1.0f) && 
			(sunraytest.CollidedPhysObj != NULL) ) 
	{ 
			PhysClass * obj = sunraytest.CollidedPhysObj;
			if (obj->Is_Casting_Shadow()) {
				obj->Inc_Ignore_Counter();
				sunresult.Reset();
				world->Cast_Ray(sunraytest);			
				obj->Dec_Ignore_Counter();
			}		
	}	
	Dec_Ignore_Counter();

	Enable_Is_In_The_Sun(sunresult.Fraction == 1.0f);
}

void PhysClass::Push_Effects(RenderInfoClass & rinfo)
{
	if (!MaterialEffectsOnMe.Is_Empty()) {
		RefMaterialEffectListIterator iterator(&MaterialEffectsOnMe);
		for ( ; !iterator.Is_Done() ; iterator.Next()) {
			iterator.Peek_Obj()->Render_Push(rinfo,this);
		}
	}

#if 0
	if (!ProjectionsOnMe.Is_Empty()) {

		ShaderClass shader = ShaderClass::_PresetOpaqueShader;
		VertexMaterialClass * vmtl = VertexMaterialClass::Get_Preset(VertexMaterialClass::PRELIT_NODIFFUSE);
		
		DX8Wrapper::Set_Shader(shader);
		DX8Wrapper::Set_Material(vmtl);

		Matrix4 view,proj;
		Matrix4 identity(true);

		DX8Wrapper::Get_Transform(D3DTS_VIEW,view);
		DX8Wrapper::Get_Transform(D3DTS_PROJECTION,proj);

		DX8Wrapper::Set_Transform(D3DTS_WORLD,identity);
		DX8Wrapper::Set_Transform(D3DTS_VIEW,identity);
		DX8Wrapper::Set_Transform(D3DTS_PROJECTION,identity);

		
		TexProjListIterator iterator(&ProjectionsOnMe);
		for ( ; !iterator.Is_Done() ; iterator.Next()) {
			TextureClass * tex = iterator.Peek_Obj()->Peek_Material_Pass()->Peek_Texture(0);
			if (tex != NULL) {
				DX8Wrapper::Set_Texture(0,tex);

				DynamicVBAccessClass vbaccess(BUFFER_TYPE_DYNAMIC_DX8,4);
				{
					DynamicVBAccessClass::WriteLockClass lock(&vbaccess);
					VertexFormatXYZNDUV2 * verts = lock.Get_Formatted_Vertex_Array();
					verts[0].x = -1.0f;
					verts[0].y = 0.8f;
					verts[0].z = 0.0;
					verts[0].u1 = 0.0f;
					verts[0].v1 = 0.0f;
					verts[0].diffuse = 0xFFFFFFFF;

					verts[1].x = -1.0f;
					verts[1].y = 0.3f;
					verts[1].z = 0.0;
					verts[1].u1 = 0.0f;
					verts[1].v1 = 1.0f;
					verts[1].diffuse = 0xFFFFFFFF;

					verts[2].x = -0.5f;
					verts[2].y = 0.3f;
					verts[2].z = 0.0;
					verts[2].u1 = 1.0f;
					verts[2].v1 = 1.0f;
					verts[2].diffuse = 0xFFFFFFFF;

					verts[3].x = -0.5f;
					verts[3].y = 0.8f;
					verts[3].z = 0.0;
					verts[3].u1 = 1.0f;
					verts[3].v1 = 0.0f;
					verts[3].diffuse = 0xFFFFFFFF;
				}

				DynamicIBAccessClass ibaccess(BUFFER_TYPE_DYNAMIC_DX8,2*3);
				{
					DynamicIBAccessClass::WriteLockClass lock(&ibaccess);
					unsigned short * indices = lock.Get_Index_Array();

					indices[0] = 0;
					indices[1] = 1;
					indices[2] = 2;
					indices[3] = 0;
					indices[4] = 2;
					indices[5] = 3;
				}

				DX8Wrapper::Set_Vertex_Buffer(vbaccess);
				DX8Wrapper::Set_Index_Buffer(ibaccess,0);
				DX8Wrapper::Draw_Triangles(0,2,0,4);
			}
		}

		DX8Wrapper::Set_Transform(D3DTS_VIEW,view);
		DX8Wrapper::Set_Transform(D3DTS_PROJECTION,proj);

		REF_PTR_RELEASE(vmtl);
	}
#endif

}

void PhysClass::Pop_Effects(RenderInfoClass & rinfo)
{
	if (!MaterialEffectsOnMe.Is_Empty()) {
		RefMaterialEffectListIterator iterator(&MaterialEffectsOnMe);
	
		while (!iterator.Is_Done()) {

			MaterialEffectClass * effect = iterator.Peek_Obj();
			effect->Render_Pop(rinfo);

			if (effect->Is_Auto_Remove_Enabled()) {
				iterator.Remove_Current_Object();
			} else {
				iterator.Next();
			}
		}
	}
}


bool PhysClass::Save (ChunkSaveClass &csave)
{
	CullableClass * cullable_ptr = (CullableClass *)this;
	WidgetUserClass * widgetuser_ptr = (WidgetUserClass *)this;
	EditableClass * editable_ptr = (EditableClass *)this;

	csave.Begin_Chunk(PHYS_CHUNK_VARIABLES);
	// (gth) not saving observer pointers any more!
	WRITE_MICRO_CHUNK_PTR(csave,PHYS_VARIABLE_CULLABLE_PTR,cullable_ptr);
	WRITE_MICRO_CHUNK_PTR(csave,PHYS_VARIABLE_WIDGETUSER_PTR,widgetuser_ptr);
	WRITE_MICRO_CHUNK_PTR(csave,PHYS_VARIABLE_EDITABLE_PTR,editable_ptr);
	WRITE_MICRO_CHUNK(csave,PHYS_VARIABLE_FLAGS,Flags);
	WRITE_MICRO_CHUNK(csave,PHYS_VARIABLE_INSTANCEID,InstanceID);	
	if (Name.Get_Length() > 0) {
		csave.Begin_Micro_Chunk(PHYS_VARIABLE_NAME);
		WWASSERT(Name.Get_Length()+1 < 255);
		csave.Write(Name,Name.Get_Length() + 1);
		csave.End_Micro_Chunk();
	}
	if (Definition != NULL) {
		int defid = Definition->Get_ID();
		WRITE_MICRO_CHUNK(csave,PHYS_VARIABLE_DEFID,defid);
	}
	csave.End_Chunk();
	
	if (Model != NULL) {
		csave.Begin_Chunk(PHYS_CHUNK_MODEL);
		csave.Begin_Chunk(Model->Get_Factory().Chunk_ID());
		Model->Get_Factory().Save(csave,Model);
		csave.End_Chunk();
		csave.End_Chunk();
	}

	if (Has_Simple_Shape()) {
		csave.Begin_Chunk(PHYS_CHUNK_SIMPLE_SHAPE);
		int simple_shape_type = static_cast<int>(SimpleShape.Type);
		WRITE_MICRO_CHUNK(csave,PHYS_SIMPLE_SHAPE_TYPE,simple_shape_type);
	if (SimpleShape.Type == PHYS_SIMPLE_SHAPE_AA_BOX) {
		csave.Begin_Micro_Chunk(PHYS_SIMPLE_SHAPE_BOX_EXTENTS);
		csave.Write(&SimpleShape.BoxHalfExtents, sizeof(SimpleShape.BoxHalfExtents));
		csave.End_Micro_Chunk();
		} else if (SimpleShape.Type == PHYS_SIMPLE_SHAPE_SPHERE) {
			WRITE_MICRO_CHUNK(csave,PHYS_SIMPLE_SHAPE_SPHERE_RADIUS,SimpleShape.SphereRadius);
		}
		csave.End_Chunk();
	}

	return true;
}

bool PhysClass::Load (ChunkLoadClass &cload)
{
	PersistFactoryClass * factory = NULL;
	CullableClass * cullable_ptr = NULL;
	WidgetUserClass * widgetuser_ptr = NULL;
	EditableClass * editable_ptr = NULL;
	int defid = -1;
	char tmpstring[256];
	tmpstring[0] = 0;
	RenderObjClass * render_model = NULL;

	while (cload.Open_Chunk()) {
		
		switch(cload.Cur_Chunk_ID()) 
		{
			case PHYS_CHUNK_VARIABLES:
				while (cload.Open_Micro_Chunk()) {
					switch(cload.Cur_Micro_Chunk_ID()) {
						// (gth) not saving observer pointers any more!
						READ_MICRO_CHUNK_PTR(cload,PHYS_VARIABLE_CULLABLE_PTR,cullable_ptr);
						READ_MICRO_CHUNK_PTR(cload,PHYS_VARIABLE_WIDGETUSER_PTR,widgetuser_ptr);
						READ_MICRO_CHUNK_PTR(cload,PHYS_VARIABLE_EDITABLE_PTR,editable_ptr);
						READ_MICRO_CHUNK(cload,PHYS_VARIABLE_FLAGS,Flags);
						READ_MICRO_CHUNK(cload,PHYS_VARIABLE_DEFID,defid);
						READ_MICRO_CHUNK(cload,PHYS_VARIABLE_INSTANCEID,InstanceID);
						
						case PHYS_VARIABLE_NAME:
							cload.Read(tmpstring,cload.Cur_Micro_Chunk_Length());
							break;
					}
					cload.Close_Micro_Chunk();	
				}
				break;

	case PHYS_CHUNK_MODEL:
		cload.Open_Chunk();
		factory = SaveLoadSystemClass::Find_Persist_Factory(cload.Cur_Chunk_ID());
		WWASSERT(factory != NULL);
		if (factory != NULL) {
			render_model = (RenderObjClass *)factory->Load(cload);
			SET_REF_OWNER(render_model);
		}
		cload.Close_Chunk();
		break;

	case PHYS_CHUNK_SIMPLE_SHAPE:
	{
		SimpleShapeDefinition loaded_shape;
		while (cload.Open_Micro_Chunk()) {
			int micro_id = cload.Cur_Micro_Chunk_ID();
			switch (micro_id) {
			case PHYS_SIMPLE_SHAPE_TYPE:
			{
				int shape_type = 0;
				cload.Read(&shape_type, sizeof(shape_type));
				loaded_shape.Type = static_cast<SimpleShapeType>(shape_type);
				break;
			}
			case PHYS_SIMPLE_SHAPE_BOX_EXTENTS:
			{
				float box_extents[3] = {0.0f, 0.0f, 0.0f};
				cload.Read(box_extents, sizeof(box_extents));
				loaded_shape.BoxHalfExtents.Set(box_extents[0], box_extents[1], box_extents[2]);
				break;
			}
			case PHYS_SIMPLE_SHAPE_SPHERE_RADIUS:
				cload.Read(&loaded_shape.SphereRadius, sizeof(loaded_shape.SphereRadius));
				break;
			default:
				cload.Seek(cload.Cur_Micro_Chunk_Length());
				break;
			}
			cload.Close_Micro_Chunk();
		}

		SimpleShape = loaded_shape;
		if (loaded_shape.Type == PHYS_SIMPLE_SHAPE_AA_BOX) {
			Set_Simple_Shape_AABox(loaded_shape.BoxHalfExtents);
		} else if (loaded_shape.Type == PHYS_SIMPLE_SHAPE_SPHERE) {
			Set_Simple_Shape_Sphere(loaded_shape.SphereRadius);
		} else {
			SimpleShape = SimpleShapeDefinition();
			Update_Cull_Box();
		}
	}
	break;

			default:
				WWDEBUG_SAY(("Unhandled Chunk: 0x%X File: %s Line: %d\r\n",cload.Cur_Chunk_ID(),__FILE__,__LINE__));
				break;
		}
		
		if (cullable_ptr != NULL) {
			SaveLoadSystemClass::Register_Pointer(cullable_ptr,(CullableClass *)this);
		}
		cload.Close_Chunk();
	}

	/*
	** Set our name
	*/
	if (strlen(tmpstring) > 0) {
		Set_Name(tmpstring);
	}

	/*
	** Set our definition pointer
	*/
	if (defid != -1) {
		Definition = (PhysDefClass *)_TheDefinitionMgr.Find_Definition(defid);	
	} else {
		Definition = NULL;
	}

	/*
	** Only install the model after our definition is installed
	*/
	Set_Model(render_model);
	REF_PTR_RELEASE(render_model);

	/*
	** Ask that our Observer pointer is re-mapped
	*/
	if (Observer != NULL) {
		REQUEST_POINTER_REMAP((void**)&Observer);
	}
	
	/*
	** Register all of the multiple-inheritance versions of our this pointer
	*/
	if (cullable_ptr != NULL) {
		SaveLoadSystemClass::Register_Pointer(cullable_ptr,(CullableClass *)this);
	}
	if (widgetuser_ptr != NULL) {
		SaveLoadSystemClass::Register_Pointer(widgetuser_ptr,(WidgetUserClass *)this);
	}
	if (editable_ptr != NULL) {
		SaveLoadSystemClass::Register_Pointer(editable_ptr,(EditableClass *)this);
	}

	/*
	** Mark our static lighting cache dirty
	*/
	Invalidate_Static_Lighting_Cache();

	/*
	** Update our Umbra object
	*/
#if (UMBRASUPPORT)
	UmbraSupport::Update_Umbra_Object(this);
#endif

	return true;
}


#ifdef WWDEBUG
void PhysClass::Add_Debug_Point(const Vector3 & p,const Vector3 & color)
{
	if (Is_Debug_Display_Enabled()) {
		if (PhysicsWorldClass * world = PhysicsWorldClass::Get_Active_World()) {
			world->Add_Debug_Point(p,color);
		}
	}
}
#endif

#ifdef WWDEBUG
void PhysClass::Add_Debug_Vector(const Vector3 & p,const Vector3 & v,const Vector3 & color)
{
	if (Is_Debug_Display_Enabled() && (v.Length2() > 0.0f)) {
		if (PhysicsWorldClass * world = PhysicsWorldClass::Get_Active_World()) {
			world->Add_Debug_Vector(p,v,color);
		}
	}
}
#endif

#ifdef WWDEBUG
void PhysClass::Add_Debug_AABox(const AABoxClass & box,const Vector3 & color,float opacity)
{
	if (Is_Debug_Display_Enabled()) {
		if (PhysicsWorldClass * world = PhysicsWorldClass::Get_Active_World()) {
			world->Add_Debug_AABox(box,color,opacity);
		}
	}
}
#endif

#ifdef WWDEBUG
void PhysClass::Add_Debug_OBBox(const OBBoxClass & box,const Vector3 & color,float opacity)
{
	if (Is_Debug_Display_Enabled()) {
		if (PhysicsWorldClass * world = PhysicsWorldClass::Get_Active_World()) {
			world->Add_Debug_OBBox(box,color,opacity);
		}
	}
}
#endif

#ifdef WWDEBUG
void PhysClass::Add_Debug_Axes(const Matrix3D & transform,const Vector3 & color)
{
	if (Is_Debug_Display_Enabled()) {
		if (PhysicsWorldClass * world = PhysicsWorldClass::Get_Active_World()) {
			world->Add_Debug_Axes(transform,color);
		}
	}
}
#endif

bool PhysClass::Is_Debug_Display_Enabled(void) const					
{ 
	const bool self_debug = ((Flags & DEBUGDISPLAY) == DEBUGDISPLAY);
	PhysicsWorldClass * world = PhysicsWorldClass::Get_Active_World();
	if (world == NULL) {
		return self_debug;
	}

	Vector3 pos;
	Get_Position(&pos);
	float dist = (pos - world->Get_Last_Camera_Position()).Length2();
	if (dist > DEBUG_RENDER_DIST2) {
		return false;
	}

	return (self_debug || world->Is_Debug_Display_Enabled());
}

bool PhysClass::Expire(void)
{
	ExpirationReactionType result = EXPIRATION_APPROVED;
	if (Observer != NULL) {
		result = Observer->Object_Expired(this);
	}
	if (result == EXPIRATION_APPROVED) {
		if (PhysicsWorldClass * world = PhysicsWorldClass::Get_Active_World()) {
			world->Delayed_Remove_Object(this);
		}
		return true;
	} else {
		return false;
	}
}

//
// TSS added this... not efficient to use if you are also 
// setting position
//
void PhysClass::Set_Facing(float new_facing)
{
	Vector3 pos;
	Get_Position(&pos);

	Matrix3D tm(1);
	tm.Translate(pos);
	tm.Rotate_Z(new_facing);
	
	Set_Transform(tm);
}

bool PhysClass::Do_Any_Effects_Suppress_Shadows(void)
{
	RefMaterialEffectListIterator iterator(&MaterialEffectsOnMe);
	for ( ; !iterator.Is_Done() ; iterator.Next()) {
		if (iterator.Peek_Obj()->Are_Shadows_Suppressed()) {
			return true;
		}
	}
	return false;
}

/***********************************************************************************************
**
** PhysDefClass Implementation
** This holds the description for a PhysClass.  Since PhysClass's aren't concrete, this
** definition class isn't either and thus has no persist factory (required by PersistClass)
** or create method (required by DefinitionClass)
**
***********************************************************************************************/

enum 
{
	PHYSDEF_CHUNK_DEFINITION		= 0x055ffe07,			// parent class data.
	PHYSDEF_CHUNK_VARIABLES,									// simple variables

	PHYSDEF_VARIABLE_FLAGS			= 0x00,
	PHYSDEF_VARIABLE_MODELNAME,
	PHYSDEF_VARIABLE_ISPRELIT,
	PHYSDEF_VARIABLE_SIMPLE_SHAPE_TYPE,
	PHYSDEF_VARIABLE_SIMPLE_SHAPE_BOX_EXTENTS,
	PHYSDEF_VARIABLE_SIMPLE_SHAPE_SPHERE_RADIUS,

};


PhysDefClass::PhysDefClass(void) : 
	ModelName ("NULL"),
	IsPreLit(false)
{
	FILENAME_PARAM(PhysDefClass,ModelName, "Westwood 3D Files", ".w3d");
	ShapeDefinition = SimpleShapeDefinition();
}

bool PhysDefClass::Is_Valid_Config (StringClass &message)
{
	bool retval = true;

	if (ModelName.Is_Empty ()) {
		if (ShapeDefinition.Type == PHYS_SIMPLE_SHAPE_NONE) {
			message += "PhysDef requires either a model name or a simple shape definition.\n";
			retval = false;
		}
	}

	if (ShapeDefinition.Type == PHYS_SIMPLE_SHAPE_AA_BOX) {
		if ((ShapeDefinition.BoxHalfExtents.X < 0.0f) ||
			(ShapeDefinition.BoxHalfExtents.Y < 0.0f) ||
			(ShapeDefinition.BoxHalfExtents.Z < 0.0f)) {
			message += "Simple shape AA box extents must be non-negative.\n";
			retval = false;
		}
	} else if (ShapeDefinition.Type == PHYS_SIMPLE_SHAPE_SPHERE) {
		if (ShapeDefinition.SphereRadius < 0.0f) {
			message += "Simple shape sphere radius must be non-negative.\n";
			retval = false;
		}
	}

	return retval;
}


bool PhysDefClass::Save(ChunkSaveClass &csave)
{
	csave.Begin_Chunk(PHYSDEF_CHUNK_DEFINITION);
	DefinitionClass::Save(csave);
	csave.End_Chunk();

	csave.Begin_Chunk(PHYSDEF_CHUNK_VARIABLES);
	WRITE_MICRO_CHUNK_WWSTRING(csave,PHYSDEF_VARIABLE_MODELNAME,ModelName);
	WRITE_MICRO_CHUNK(csave,PHYSDEF_VARIABLE_ISPRELIT,IsPreLit);	
	int def_shape_type = static_cast<int>(ShapeDefinition.Type);
	WRITE_MICRO_CHUNK(csave,PHYSDEF_VARIABLE_SIMPLE_SHAPE_TYPE,def_shape_type);
	if (ShapeDefinition.Type == PHYS_SIMPLE_SHAPE_AA_BOX) {
		csave.Begin_Micro_Chunk(PHYSDEF_VARIABLE_SIMPLE_SHAPE_BOX_EXTENTS);
		csave.Write(&ShapeDefinition.BoxHalfExtents, sizeof(ShapeDefinition.BoxHalfExtents));
		csave.End_Micro_Chunk();
	} else if (ShapeDefinition.Type == PHYS_SIMPLE_SHAPE_SPHERE) {
		WRITE_MICRO_CHUNK(csave,PHYSDEF_VARIABLE_SIMPLE_SHAPE_SPHERE_RADIUS,ShapeDefinition.SphereRadius);
	}
	csave.End_Chunk();
	return true;

}

bool PhysDefClass::Load(ChunkLoadClass &cload)
{
	while (cload.Open_Chunk()) {

		switch(cload.Cur_Chunk_ID()) {			

			case PHYSDEF_CHUNK_DEFINITION:
				DefinitionClass::Load(cload);
				break;

	case PHYSDEF_CHUNK_VARIABLES:
		WWASSERT(cload.Cur_Chunk_ID() == PHYSDEF_CHUNK_VARIABLES);
		while (cload.Open_Micro_Chunk()) {
			int micro_id = cload.Cur_Micro_Chunk_ID();
			switch (micro_id) {
			case PHYSDEF_VARIABLE_MODELNAME:
			{
				uint32 length = cload.Cur_Micro_Chunk_Length();
				std::string temp;
				temp.resize(length ? length - 1 : 0);
				if (length) {
					if (!temp.empty()) {
						cload.Read(temp.data(), temp.size());
					}
					char terminator = 0;
					cload.Read(&terminator, sizeof(terminator));
				}
				ModelName = temp.c_str();
				break;
			}
			case PHYSDEF_VARIABLE_ISPRELIT:
				cload.Read(&IsPreLit, sizeof(IsPreLit));
				break;
			case PHYSDEF_VARIABLE_SIMPLE_SHAPE_TYPE:
			{
				int type_value = 0;
				cload.Read(&type_value, sizeof(type_value));
				ShapeDefinition.Type = static_cast<PhysSimpleShapeType>(type_value);
				break;
			}
			case PHYSDEF_VARIABLE_SIMPLE_SHAPE_BOX_EXTENTS:
			{
				float def_extents[3] = {0.0f, 0.0f, 0.0f};
				cload.Read(def_extents, sizeof(def_extents));
				ShapeDefinition.BoxHalfExtents.Set(def_extents[0], def_extents[1], def_extents[2]);
				break;
			}
			case PHYSDEF_VARIABLE_SIMPLE_SHAPE_SPHERE_RADIUS:
				cload.Read(&ShapeDefinition.SphereRadius, sizeof(ShapeDefinition.SphereRadius));
				break;
			default:
				cload.Seek(cload.Cur_Micro_Chunk_Length());
				break;
			}
			cload.Close_Micro_Chunk();
		}
		break;
		}

		cload.Close_Chunk();
	}
	return true;
}

bool PhysDefClass::Is_Type(const char * type_name)
{
	if (stricmp(type_name,PhysDefClass::Get_Type_Name()) == 0) {
		return true;
	} else {
		return false;
	}
}
