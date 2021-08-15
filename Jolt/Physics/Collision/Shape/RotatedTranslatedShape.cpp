// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#include <Jolt.h>

#include <Physics/Collision/Shape/RotatedTranslatedShape.h>
#include <Physics/Collision/CollisionDispatch.h>
#include <Physics/Collision/RayCast.h>
#include <Physics/Collision/ShapeCast.h>
#include <Physics/Collision/TransformedShape.h>
#include <Core/StreamIn.h>
#include <Core/StreamOut.h>
#include <ObjectStream/TypeDeclarations.h>

namespace JPH {

JPH_IMPLEMENT_SERIALIZABLE_VIRTUAL(RotatedTranslatedShapeSettings)
{
	JPH_ADD_BASE_CLASS(RotatedTranslatedShapeSettings, DecoratedShapeSettings)

	JPH_ADD_ATTRIBUTE(RotatedTranslatedShapeSettings, mPosition)
	JPH_ADD_ATTRIBUTE(RotatedTranslatedShapeSettings, mRotation)
}

JPH_IMPLEMENT_RTTI_VIRTUAL(RotatedTranslatedShape)
{
	JPH_ADD_BASE_CLASS(RotatedTranslatedShape, DecoratedShape)
}

ShapeSettings::ShapeResult RotatedTranslatedShapeSettings::Create() const
{ 
	if (mCachedResult.IsEmpty())
		Ref<Shape> shape = new RotatedTranslatedShape(*this, mCachedResult); 
	return mCachedResult;
}

RotatedTranslatedShape::RotatedTranslatedShape(const RotatedTranslatedShapeSettings &inSettings, ShapeResult &outResult) :
	DecoratedShape(inSettings, outResult)
{
	if (outResult.HasError())
		return;

	// Calculate center of mass position
	mCenterOfMass = inSettings.mPosition + inSettings.mRotation * mInnerShape->GetCenterOfMass(); 

	// Store rotation (position is always zero because we center around the center of mass)
	mRotation = inSettings.mRotation;
	mIsRotationIdentity = mRotation.IsClose(Quat::sIdentity());

	outResult.Set(this);
}

MassProperties RotatedTranslatedShape::GetMassProperties() const
{
	// Rotate inertia of child into place
	MassProperties p = mInnerShape->GetMassProperties();
	p.Rotate(Mat44::sRotation(mRotation));
	return p;
}

AABox RotatedTranslatedShape::GetLocalBounds() const
{
	return mInnerShape->GetLocalBounds().Transformed(Mat44::sRotation(mRotation));
}

AABox RotatedTranslatedShape::GetWorldSpaceBounds(Mat44Arg inCenterOfMassTransform, Vec3Arg inScale) const
{ 
	Mat44 transform = inCenterOfMassTransform * Mat44::sRotation(mRotation);
	return mInnerShape->GetWorldSpaceBounds(transform, TransformScale(inScale));
}

TransformedShape RotatedTranslatedShape::GetSubShapeTransformedShape(const SubShapeID &inSubShapeID, Vec3Arg inPositionCOM, QuatArg inRotation, Vec3Arg inScale, SubShapeID &outRemainder) const
{
	// We don't use any bits in the sub shape ID
	outRemainder = inSubShapeID;

	TransformedShape ts(inPositionCOM, inRotation * mRotation, mInnerShape, BodyID());
	ts.SetShapeScale(TransformScale(inScale));
	return ts;
}

Vec3 RotatedTranslatedShape::GetSurfaceNormal(const SubShapeID &inSubShapeID, Vec3Arg inLocalSurfacePosition) const 
{ 
	// Transform surface position to local space and pass call on
	Mat44 transform = Mat44::sRotation(mRotation.Conjugated());
	Vec3 normal = mInnerShape->GetSurfaceNormal(inSubShapeID, transform * inLocalSurfacePosition);

	// Transform normal to this shape's space
	return transform.Multiply3x3Transposed(normal);
}

void RotatedTranslatedShape::GetSubmergedVolume(Mat44Arg inCenterOfMassTransform, Vec3Arg inScale, const Plane &inSurface, float &outTotalVolume, float &outSubmergedVolume, Vec3 &outCenterOfBuoyancy) const
{
	// Get center of mass transform of child
	Mat44 transform = inCenterOfMassTransform * Mat44::sRotation(mRotation);

	// Recurse to child
	mInnerShape->GetSubmergedVolume(transform, TransformScale(inScale), inSurface, outTotalVolume, outSubmergedVolume, outCenterOfBuoyancy);
}

#ifdef JPH_DEBUG_RENDERER
void RotatedTranslatedShape::Draw(DebugRenderer *inRenderer, Mat44Arg inCenterOfMassTransform, Vec3Arg inScale, ColorArg inColor, bool inUseMaterialColors, bool inDrawWireframe) const
{
	mInnerShape->Draw(inRenderer, inCenterOfMassTransform * Mat44::sRotation(mRotation), TransformScale(inScale), inColor, inUseMaterialColors, inDrawWireframe);
}

void RotatedTranslatedShape::DrawGetSupportFunction(DebugRenderer *inRenderer, Mat44Arg inCenterOfMassTransform, Vec3Arg inScale, ColorArg inColor, bool inDrawSupportDirection) const
{
	mInnerShape->DrawGetSupportFunction(inRenderer, inCenterOfMassTransform * Mat44::sRotation(mRotation), TransformScale(inScale), inColor, inDrawSupportDirection);
}

void RotatedTranslatedShape::DrawGetSupportingFace(DebugRenderer *inRenderer, Mat44Arg inCenterOfMassTransform, Vec3Arg inScale) const
{
	mInnerShape->DrawGetSupportingFace(inRenderer, inCenterOfMassTransform * Mat44::sRotation(mRotation), TransformScale(inScale));
}
#endif // JPH_DEBUG_RENDERER

bool RotatedTranslatedShape::CastRay(const RayCast &inRay, const SubShapeIDCreator &inSubShapeIDCreator, RayCastResult &ioHit) const
{	
	// Transform the ray
	Mat44 transform = Mat44::sRotation(mRotation.Conjugated());
	RayCast ray = inRay.Transformed(transform);

	return mInnerShape->CastRay(ray, inSubShapeIDCreator, ioHit);
}

void RotatedTranslatedShape::CastRay(const RayCast &inRay, const RayCastSettings &inRayCastSettings, const SubShapeIDCreator &inSubShapeIDCreator, CastRayCollector &ioCollector) const
{
	// Transform the ray
	Mat44 transform = Mat44::sRotation(mRotation.Conjugated());
	RayCast ray = inRay.Transformed(transform);

	return mInnerShape->CastRay(ray, inRayCastSettings, inSubShapeIDCreator, ioCollector);
}

void RotatedTranslatedShape::CollidePoint(Vec3Arg inPoint, const SubShapeIDCreator &inSubShapeIDCreator, CollidePointCollector &ioCollector) const
{
	// Transform the point
	Mat44 transform = Mat44::sRotation(mRotation.Conjugated());
	mInnerShape->CollidePoint(transform * inPoint, inSubShapeIDCreator, ioCollector);
}

void RotatedTranslatedShape::CastShape(const ShapeCast &inShapeCast, const ShapeCastSettings &inShapeCastSettings, Vec3Arg inScale, const ShapeFilter &inShapeFilter, Mat44Arg inCenterOfMassTransform2, const SubShapeIDCreator &inSubShapeIDCreator1, const SubShapeIDCreator &inSubShapeIDCreator2, CastShapeCollector &ioCollector) const 
{
	// Determine the local transform
	Mat44 local_transform = Mat44::sRotation(mRotation);

	// Transform the shape cast
	ShapeCast shape_cast = inShapeCast.PostTransformed(local_transform.Transposed3x3());

	mInnerShape->CastShape(shape_cast, inShapeCastSettings, TransformScale(inScale), inShapeFilter, inCenterOfMassTransform2 * local_transform, inSubShapeIDCreator1, inSubShapeIDCreator2, ioCollector);
}

void RotatedTranslatedShape::CollectTransformedShapes(const AABox &inBox, Vec3Arg inPositionCOM, QuatArg inRotation, Vec3Arg inScale, const SubShapeIDCreator &inSubShapeIDCreator, TransformedShapeCollector &ioCollector) const
{
	mInnerShape->CollectTransformedShapes(inBox, inPositionCOM, inRotation * mRotation, TransformScale(inScale), inSubShapeIDCreator, ioCollector);
}

void RotatedTranslatedShape::TransformShape(Mat44Arg inCenterOfMassTransform, TransformedShapeCollector &ioCollector) const
{
	mInnerShape->TransformShape(inCenterOfMassTransform * Mat44::sRotation(mRotation), ioCollector);
}

void RotatedTranslatedShape::sCollideRotatedTranslatedVsShape(const RotatedTranslatedShape *inShape1, const Shape *inShape2, Vec3Arg inScale1, Vec3Arg inScale2, Mat44Arg inCenterOfMassTransform1, Mat44Arg inCenterOfMassTransform2, const SubShapeIDCreator &inSubShapeIDCreator1, const SubShapeIDCreator &inSubShapeIDCreator2, const CollideShapeSettings &inCollideShapeSettings, CollideShapeCollector &ioCollector)
{	
	// Get world transform of 1
	Mat44 transform1 = inCenterOfMassTransform1 * Mat44::sRotation(inShape1->mRotation);

	CollisionDispatch::sCollideShapeVsShape(inShape1->mInnerShape, inShape2, inShape1->TransformScale(inScale1), inScale2, transform1, inCenterOfMassTransform2, inSubShapeIDCreator1, inSubShapeIDCreator2, inCollideShapeSettings, ioCollector);
}

void RotatedTranslatedShape::sCollideShapeVsRotatedTranslated(const Shape *inShape1, const RotatedTranslatedShape *inShape2, Vec3Arg inScale1, Vec3Arg inScale2, Mat44Arg inCenterOfMassTransform1, Mat44Arg inCenterOfMassTransform2, const SubShapeIDCreator &inSubShapeIDCreator1, const SubShapeIDCreator &inSubShapeIDCreator2, const CollideShapeSettings &inCollideShapeSettings, CollideShapeCollector &ioCollector)
{
	// Get world transform of 2
	Mat44 transform2 = inCenterOfMassTransform2 * Mat44::sRotation(inShape2->mRotation);

	CollisionDispatch::sCollideShapeVsShape(inShape1, inShape2->mInnerShape, inScale1, inShape2->TransformScale(inScale2), inCenterOfMassTransform1, transform2, inSubShapeIDCreator1, inSubShapeIDCreator2, inCollideShapeSettings, ioCollector);
}

void RotatedTranslatedShape::sCastRotatedTranslatedShapeVsShape(const ShapeCast &inShapeCast, const ShapeCastSettings &inShapeCastSettings, const Shape *inShape, Vec3Arg inScale, const ShapeFilter &inShapeFilter, Mat44Arg inCenterOfMassTransform2, const SubShapeIDCreator &inSubShapeIDCreator1, const SubShapeIDCreator &inSubShapeIDCreator2, CastShapeCollector &ioCollector)
{
	// Fetch rotated translated shape from cast shape
	JPH_ASSERT(inShapeCast.mShape->GetType() == EShapeType::RotatedTranslated);
	const RotatedTranslatedShape *shape1 = static_cast<const RotatedTranslatedShape *>(inShapeCast.mShape.GetPtr());

	// Transform the shape cast and update the shape
	Mat44 transform = inShapeCast.mCenterOfMassStart * Mat44::sRotation(shape1->mRotation);
	Vec3 scale = shape1->TransformScale(inShapeCast.mScale);
	ShapeCast shape_cast(shape1->mInnerShape, scale, transform, inShapeCast.mDirection);

	CollisionDispatch::sCastShapeVsShape(shape_cast, inShapeCastSettings, inShape, inScale, inShapeFilter, inCenterOfMassTransform2, inSubShapeIDCreator1, inSubShapeIDCreator2, ioCollector);
}

void RotatedTranslatedShape::SaveBinaryState(StreamOut &inStream) const
{
	DecoratedShape::SaveBinaryState(inStream);

	inStream.Write(mCenterOfMass);
	inStream.Write(mRotation);
}

void RotatedTranslatedShape::RestoreBinaryState(StreamIn &inStream)
{
	DecoratedShape::RestoreBinaryState(inStream);

	inStream.Read(mCenterOfMass);
	inStream.Read(mRotation);
	mIsRotationIdentity = mRotation.IsClose(Quat::sIdentity());
}

bool RotatedTranslatedShape::IsValidScale(Vec3Arg inScale) const
{
	if (!DecoratedShape::IsValidScale(inScale))
		return false;

	if (mIsRotationIdentity || ScaleHelpers::IsUniformScale(inScale))
		return mInnerShape->IsValidScale(inScale);

	if (!ScaleHelpers::CanScaleBeRotated(mRotation, inScale))
		return false;

	return mInnerShape->IsValidScale(ScaleHelpers::RotateScale(mRotation, inScale));
}

} // JPH