varying mediump vec3 vWorldPosition;
varying mediump vec3 vViewPosition;
varying mediump vec3 vNormal;
varying mediump vec3 vReflect;

uniform mat4 viewMatrix;

uniform samplerCube envMap;

/*
uniform vec3 diffuse;
uniform float opacity;
uniform vec3 ambient;
uniform vec3 emissive;
uniform vec3 specular;
uniform float shininess;
uniform vec3 ambientLightColor;

uniform vec3 directionalLightColor;
uniform vec3 directionalLightDirection;

uniform vec3 spotLightColor;
uniform vec3 spotLightPosition;
uniform vec3 spotLightDirection;
uniform float spotLightAngleCos;
uniform float spotLightExponent;
uniform float spotLightDistance;
*/

uniform lowp vec4 tint;

uniform mediump vec4 lightPosition;
uniform mediump vec4 lightDirection;

void main() {
	lowp vec3 diffuse = tint.xyz;
	lowp float opacity = tint.w;
	vec3 ambient = vec3(1.0);
	vec3 emissive = vec3(0.0);
	vec3 specular = vec3(1.0); // 0.8 and 0.25 also used
	float shininess = 30.0;
	vec3 ambientLightColor = vec3(0.0);
	
	vec3 directionalLightColor = vec3(1.0);
	vec3 directionalLightDirection = vec3(0.0, 1.0, 0.0);
	
	vec3 spotLightColor = vec3(1.0);
	vec3 spotLightPosition = lightPosition.xyz;
	vec3 spotLightDirection = lightDirection.xyz;
	float spotLightAngleCos = 0.0;
	float spotLightExponent = 10.0;
	float spotLightDistance = 1000.0;
	
	float reflectivity = 0.5;

	gl_FragColor = vec4( vec3 ( 1.0 ), opacity );
	float specularStrength;
	specularStrength = 1.0;
	vec3 normal = normalize( vNormal );
	vec3 viewPosition = normalize( vViewPosition );
	vec3 spotDiffuse  = vec3( 0.0 );
	vec3 spotSpecular = vec3( 0.0 );
	vec4 lPosition = viewMatrix * vec4( spotLightPosition, 1.0 );
	vec3 lVector = lPosition.xyz + vViewPosition.xyz;
	float lDistance = 1.0;
	if ( spotLightDistance > 0.0 ) {
		lDistance = 1.0 - min( ( length( lVector ) / spotLightDistance ), 1.0 );
	}
	lVector = normalize( lVector );
	float spotEffect = dot( spotLightDirection, normalize( spotLightPosition - vWorldPosition ) );
	if ( spotEffect > spotLightAngleCos ) {
		spotEffect = max( pow( spotEffect, spotLightExponent ), 0.0 );
		float dotProduct = dot( normal, lVector );
		float spotDiffuseWeight = max( dotProduct, 0.0 );
		spotDiffuse += diffuse * spotLightColor * spotDiffuseWeight * lDistance * spotEffect;
		vec3 spotHalfVector = normalize( lVector + viewPosition );
		float spotDotNormalHalf = max( dot( normal, spotHalfVector ), 0.0 );
		float spotSpecularWeight = specularStrength * max( pow( spotDotNormalHalf, shininess ), 0.0 );
		spotSpecular += specular * spotLightColor * spotSpecularWeight * spotDiffuseWeight * lDistance * spotEffect;
	}
	vec3 dirDiffuse  = vec3( 0.0 );
	vec3 dirSpecular = vec3( 0.0 );
	vec4 lDirection = viewMatrix * vec4( directionalLightDirection, 0.0 );
	vec3 dirVector = normalize( lDirection.xyz );
	float dotProduct = dot( normal, dirVector );
	float dirDiffuseWeight = max( dotProduct, 0.0 );
	dirDiffuse  += diffuse * directionalLightColor * dirDiffuseWeight;
	vec3 dirHalfVector = normalize( dirVector + viewPosition );
	float dirDotNormalHalf = max( dot( normal, dirHalfVector ), 0.0 );
	float dirSpecularWeight = specularStrength * max( pow( dirDotNormalHalf, shininess ), 0.0 );
	dirSpecular += specular * directionalLightColor * dirSpecularWeight * dirDiffuseWeight;
	vec3 totalDiffuse = vec3( 0.0 );
	vec3 totalSpecular = vec3( 0.0 );
	totalDiffuse += dirDiffuse;
	totalSpecular += dirSpecular;
	totalDiffuse += spotDiffuse;
	totalSpecular += spotSpecular;
	gl_FragColor.xyz = gl_FragColor.xyz * ( emissive + totalDiffuse + ambientLightColor * ambient + totalSpecular);

	vec4 cubeColor = textureCube(envMap, vReflect);
	gl_FragColor.xyz = mix( gl_FragColor.xyz, cubeColor.xyz, specularStrength * reflectivity );
}