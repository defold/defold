components
{
    id: "script"
    component: "attacker_target/attacker.script"
}
components
{
    id: "co"
    component: "attacker_target/attacker.collisionobject"
}
components
{
    id: "shot"
    component: "attacker_target/shot.wav"
}
embedded_components
{
    id: "bullet_spawner"
    type: "spawnpoint"
    data: "prototype: \"attacker_target/bullet.go\""
}
