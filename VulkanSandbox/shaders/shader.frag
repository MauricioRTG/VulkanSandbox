#version 450

//Descriptor
layout(binding = 1) uniform sampler2D texSampler;

//Vertex buffer
layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragTexCoord;

//Output
layout(location = 0) out vec4 outColor;

void main(){
	//outColor = vec4(fragTexCoord, 0.0, 1.0);
	//Texture is sampled: It takes a sampler and coordinate as arguments. The sampler automatically takes care of the filtering and transformations in the background.
	//outColor = texture(texSampler, fragTexCoord * 2.0);
	outColor = vec4(fragColor * texture(texSampler, fragTexCoord).rgb, 1.0);
}