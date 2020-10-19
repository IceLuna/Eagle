#include "Eagle.h"

class Sandbox : public Eagle::Application
{

};

Eagle::Application* Eagle::CreateApplication()
{
	return new Sandbox;
}

