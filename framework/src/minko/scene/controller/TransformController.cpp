#include "TransformController.hpp"
#include "minko/math/Matrix4x4.hpp"
#include "minko/scene/Node.hpp"
#include "minko/scene/NodeSet.hpp"

using namespace minko::scene::controller;
using namespace minko::scene::data;

TransformController::TransformController() :
	minko::scene::controller::AbstractController(),
	_transform(Matrix4x4::create()),
	_modelToWorld(Matrix4x4::create()),
	_data(DataProvider::create())
{
}

void
TransformController::initialize()
{
	_targetAddedCd = targetAdded()->add(std::bind(
		&TransformController::targetAddedHandler,
		shared_from_this(),
		std::placeholders::_1,
		std::placeholders::_2
	));

	_targetRemovedCd = targetRemoved()->add(std::bind(
		&TransformController::targetRemovedHandler,
		shared_from_this(),
		std::placeholders::_1,
		std::placeholders::_2
	));

	_data->setProperty("transforms/modelToWorldMatrix", _modelToWorld);
	_data->setProperty("transforms/worldToModelMatrix", _worldToModel);
}

void
TransformController::targetAddedHandler(std::shared_ptr<AbstractController> ctrl,
										std::shared_ptr<Node> 				target)
{
	if (targets().size() > 1)
		throw std::logic_error("TransformController cannot have more than one target.");
	if (target->controller<TransformController>(1) != nullptr)
		throw std::logic_error("A node cannot have more than one TransformController.");

	target->bindings()->addProvider(_data);
	
	auto callback = std::bind(
		&TransformController::addedOrRemovedHandler,
		shared_from_this(),
		std::placeholders::_1,
		std::placeholders::_2
	);

	_addedCd = target->added()->add(callback);
	_removedCd = target->removed()->add(callback);

	addedOrRemovedHandler(target, target);
}

void
TransformController::addedOrRemovedHandler(std::shared_ptr<Node> node, std::shared_ptr<Node> ancestor)
{
	if (node->parent() && !node->root()->controller<RootTransformController>())
		node->root()->addController(RootTransformController::create());
}

void
TransformController::targetRemovedHandler(std::shared_ptr<AbstractController> 	ctrl,
										  std::shared_ptr<Node> 				target)
{
	target->bindings()->removeProvider(_data);

	target->added()->remove(_addedCd);
	target->removed()->remove(_removedCd);
}

void
TransformController::RootTransformController::initialize()
{
	_targetCds.push_back(targetAdded()->add(std::bind(
		&TransformController::RootTransformController::targetAddedHandler,
		shared_from_this(),
		std::placeholders::_1,
		std::placeholders::_2
	)));

	_targetCds.push_back(targetRemoved()->add(std::bind(
		&TransformController::RootTransformController::targetRemovedHandler,
		shared_from_this(),
		std::placeholders::_1,
		std::placeholders::_2
	)));
}

void
TransformController::RootTransformController::targetAddedHandler(std::shared_ptr<AbstractController> 	ctrl,
																 std::shared_ptr<Node>					target)
{
	_targetCds.push_back(target->descendantAdded()->add(std::bind(
		&TransformController::RootTransformController::descendantAddedHandler,
		shared_from_this(),
		std::placeholders::_1,
		std::placeholders::_2
	)));
	_targetCds.push_back(target->descendantRemoved()->add(std::bind(
		&TransformController::RootTransformController::descendantRemovedHandler,
		shared_from_this(),
		std::placeholders::_1,
		std::placeholders::_2
	)));
	_targetCds.push_back(target->controllerAdded()->add(std::bind(
		&TransformController::RootTransformController::controllerAddedHandler,
		shared_from_this(),
		std::placeholders::_1,
		std::placeholders::_2
	)));
	_targetCds.push_back(target->controllerRemoved()->add(std::bind(
		&TransformController::RootTransformController::controllerRemovedHandler,
		shared_from_this(),
		std::placeholders::_1,
		std::placeholders::_2
	)));

	descendantAddedHandler(target, target);
}

void
TransformController::RootTransformController::targetRemovedHandler(std::shared_ptr<AbstractController> 	ctrl,
																   std::shared_ptr<Node>				target)
{
	_targetCds.clear();
	_enterFrameCds.clear();
}

void
TransformController::RootTransformController::controllerAddedHandler(std::shared_ptr<Node>					node,
														 			 std::shared_ptr<AbstractController>	ctrl)
{
	auto renderingCtrl = std::dynamic_pointer_cast<RenderingController>(ctrl);

	if (renderingCtrl != nullptr)
		_enterFrameCds[renderingCtrl] = renderingCtrl->enterFrame()->add(std::bind(
			&TransformController::RootTransformController::enterFrameHandler,
			shared_from_this(),
			std::placeholders::_1
		));
	else if (std::dynamic_pointer_cast<TransformController>(ctrl) != nullptr)
		_invalidLists = true;
}

void
TransformController::RootTransformController::controllerRemovedHandler(std::shared_ptr<Node>				node,
																	   std::shared_ptr<AbstractController>	ctrl)
{
	auto renderingCtrl = std::dynamic_pointer_cast<RenderingController>(ctrl);

	if (renderingCtrl != nullptr)
		_enterFrameCds.erase(renderingCtrl);
	else if (std::dynamic_pointer_cast<TransformController>(ctrl) != nullptr)
		_invalidLists = true;	
}

void
TransformController::RootTransformController::descendantAddedHandler(std::shared_ptr<Node> node,
														 			 std::shared_ptr<Node> descendant)
{
	auto enterFrameCallback = std::bind(
		&TransformController::RootTransformController::enterFrameHandler,
		shared_from_this(),
		std::placeholders::_1
	);

	auto descendants = NodeSet::create(descendant)->descendants(true);
	for (auto d : descendants->nodes())
	{
		auto rootTransformCtrl = d->controller<RootTransformController>();

		if (rootTransformCtrl && rootTransformCtrl != shared_from_this())
			d->removeController(rootTransformCtrl);

		for (auto renderingCtrl : d->controllers<RenderingController>())
			_enterFrameCds[renderingCtrl] = renderingCtrl->enterFrame()->add(enterFrameCallback);
	}

	_invalidLists = true;
}

void
TransformController::RootTransformController::descendantRemovedHandler(std::shared_ptr<Node> node,
																	   std::shared_ptr<Node> descendant)
{
	auto descendants = NodeSet::create(descendant)->descendants(true);
	for (auto d : descendants->nodes())
		for (auto renderingCtrl : d->controllers<RenderingController>())
			_enterFrameCds.erase(renderingCtrl);

	_invalidLists = true;
}

void
TransformController::RootTransformController::updateTransformsList()
{
	unsigned int nodeId = 0;

	_transform.clear();
	_modelToWorld.clear();
	//_worldToModel.clear();

	auto descendants = NodeSet::create(targets())
		->descendants(true, false)
		->where([](std::shared_ptr<Node> node)
			{
				return node->controller<TransformController>() != nullptr;
			});

	for (auto node : descendants->nodes())
	{
		auto transformCtrl  = node->controller<TransformController>();

		_nodeToId[node] = nodeId;

		_idToNode.push_back(node);
		_transform.push_back(transformCtrl->_transform);
		_modelToWorld.push_back(transformCtrl->_modelToWorld);
		_numChildren.push_back(0);
		_firstChildId.push_back(0);

		auto ancestor = node->parent();
		while (ancestor != nullptr && _nodeToId.count(ancestor) == 0)
			ancestor = ancestor->parent();

		if (ancestor != nullptr)
		{
			auto ancestorId = _nodeToId[ancestor];

			if (_numChildren[ancestorId] == 0)
				_firstChildId[ancestorId] = nodeId;
			_numChildren[ancestorId]++;
		}

		++nodeId;
	}

	_invalidLists = false;
}

void
TransformController::RootTransformController::updateTransforms()
{
	auto numNodes 	= _transform.size();
	auto nodeId 	= 0;

	while (nodeId < numNodes)
	{
		auto parentModelToWorldMatrix 	= _modelToWorld[nodeId];
		auto numChildren 				= _numChildren[nodeId];
		auto firstChildId 				= _firstChildId[nodeId];
		auto lastChildId 				= firstChildId + numChildren;

		for (auto childId = firstChildId; childId < lastChildId; ++childId)
			_modelToWorld[childId]->copyFrom(_transform[childId])->append(parentModelToWorldMatrix);

		++nodeId;
	}
}

void
TransformController::RootTransformController::enterFrameHandler(std::shared_ptr<RenderingController> ctrl)
{
	if (_invalidLists)
		updateTransformsList();
	
	updateTransforms();
}