def String get_megachat_src(String image_tag, String intree) {
    return intree.equals('true') ? "${WORKSPACE}/${image_tag}/in_tree/megachat" : "${WORKSPACE}/${image_tag}/out_tree/megachat"
}

def String get_sdk_src(String image_tag, String intree) {
    return intree.equals('true') ? "${WORKSPACE}/${image_tag}/in_tree/megachat/third-party/mega" : "${WORKSPACE}/${image_tag}/out_tree/sdk"
}

def String get_image_name(String image_tag, String intree) {
    def tree_label = intree.equals('true') ? "in" : "out"
    return "megachat-linux-build-${tree_label}:${image_tag}"
}

def String get_sdk_volume_parameter(String image_tag, String intree) {
    def sdk_sources_workspace = get_sdk_src(image_tag, intree)
    return intree.equals('true') ? " " : "-v ${sdk_sources_workspace}:/mega/sdk"
}

pipeline {
    agent { label 'linux && docker' }
    options {
        timeout(time: 300, unit: 'MINUTES')
        buildDiscarder(logRotator(numToKeepStr: '135', daysToKeepStr: '21'))
        gitLabConnection('GitLabConnectionJenkins')
    }
    parameters {
        booleanParam(name: 'RESULT_TO_SLACK', defaultValue: true, description: 'Should the job result be sent to slack?')
        booleanParam(name: 'ubuntu_2204_in_tree', defaultValue: true, description: 'Build in Ubuntu 22.04. SDK inside tree')
        booleanParam(name: 'ubuntu_2404_in_tree', defaultValue: true, description: 'Build in Ubuntu 24.04. SDK inside tree')
        booleanParam(name: 'ubuntu_2204_out_tree', defaultValue: false, description: 'Build in Ubuntu 22.04. SDK outside tree')
        booleanParam(name: 'ubuntu_2404_out_tree', defaultValue: true, description: 'Build in Ubuntu 24.04. SDK outside tree')
        string(name: 'SDK_BRANCH', defaultValue: 'develop', description: 'Define a custom SDK branch.')
        string(name: 'MEGACHAT_BRANCH', defaultValue: 'develop', description: 'Define a custom MEGAchat branch.')
    }
    environment {
        SDK_BRANCH = "${params.SDK_BRANCH}"
        MEGACHAT_BRANCH = "${params.MEGACHAT_BRANCH}"
    }
    stages {
        stage('Nightly build'){
            matrix {
                axes {
                    axis {
                        name 'IMAGE_TAG'
                        values 'ubuntu-2204' , 'ubuntu-2404'
                    }
                    axis {
                        name 'IN_TREE'
                        values true, false
                    }
                }
                when {
                    anyOf {
                        expression { IMAGE_TAG == 'ubuntu-2204' && IN_TREE == 'true' && params.ubuntu_2204_in_tree }
                        expression { IMAGE_TAG == 'ubuntu-2404' && IN_TREE == 'true' && params.ubuntu_2404_in_tree }
                        expression { IMAGE_TAG == 'ubuntu-2204' && IN_TREE == 'false' && params.ubuntu_2204_out_tree }
                        expression { IMAGE_TAG == 'ubuntu-2404' && IN_TREE == 'false' && params.ubuntu_2404_out_tree }
                    }
                }
                environment {
                    megachat_sources_workspace = get_megachat_src(IMAGE_TAG, IN_TREE)
                    sdk_sources_workspace = get_sdk_src(IMAGE_TAG, IN_TREE)
                    image_name = get_image_name(IMAGE_TAG, IN_TREE)
                    sdk_volume = get_sdk_volume_parameter(IMAGE_TAG, IN_TREE)
                }
                stages{
                    stage('checkout'){
                        steps {
                            dir(megachat_sources_workspace){
                                checkout([
                                    $class: 'GitSCM',
                                    branches: [[name: "${MEGACHAT_BRANCH}"]],
                                        userRemoteConfigs: [[ url: "${GIT_URL_MEGACHAT}", credentialsId: "12492eb8-0278-4402-98f0-4412abfb65c1" ]],
                                        extensions: [
                                            [$class: "UserIdentity",name: "jenkins", email: "jenkins@jenkins"]
                                        ]
                                    ])
                            }
                            dir(sdk_sources_workspace){
                                checkout([
                                    $class: 'GitSCM',
                                    branches: [[name: "${SDK_BRANCH}"]],
                                    userRemoteConfigs: [[ url: "${GIT_URL_SDK}", credentialsId: "12492eb8-0278-4402-98f0-4412abfb65c1" ]],
                                    extensions: [
                                        [$class: "UserIdentity",name: "jenkins", email: "jenkins@jenkins"]
                                    ]
                                ])
                            }
                        }
                    }
                    stage("Build image") {
                        steps {
                            sh """
                                case "${IMAGE_TAG}" in
                                    ubuntu-2204)
                                        DISTRO='ubuntu:22.04'
                                        ;;
                                    ubuntu-2404)
                                        DISTRO='ubuntu:24.04'
                                        ;;
                                esac
                                docker build \
                                    --build-arg DISTRO=\${DISTRO} \
                                    -f ${megachat_sources_workspace}/dockerfile/linux-build.dockerfile \
                                    -t ${image_name} \
                                    ${megachat_sources_workspace} \
                                    --no-cache --pull --progress=plain
                            """
                        }
                    }
                    stage("Build MEGAchat") {
                        steps {
                            sh """
                                 docker run \
                                    --rm \
                                    -v ${megachat_sources_workspace}:/mega/MEGAchat \
                                    ${sdk_volume} \
                                    -e VCPKG_BINARY_SOURCES \
                                    -e AWS_ACCESS_KEY_ID \
                                    -e AWS_SECRET_ACCESS_KEY \
                                    -e AWS_ENDPOINT_URL \
                                    ${image_name}
                            """
                        }
                    }
                }
                post {
                    always {
                        sh "docker rmi -f ${image_name}"
                    }
                }
            }
        }
    }
    post {
        always {
            script {
                if (params.RESULT_TO_SLACK) {
                    def megachat_sources_workspace = get_megachat_src("ubuntu-2204", "false")
                    def sdk_sources_workspace = get_sdk_src("ubuntu-2204", "false")
                    def megachat_commit = sh(script: "git -C ${megachat_sources_workspace} rev-parse HEAD", returnStdout: true).trim()
                    def sdk_commit = sh(script: "git -C ${sdk_sources_workspace} rev-parse HEAD", returnStdout: true).trim()
                    def messageStatus = currentBuild.currentResult
                    def messageColor = messageStatus == 'SUCCESS'? "#00FF00": "#FF0000" //green or red
                    message = """
                        *MEGAchat Linux* <${BUILD_URL}|Build result>: '${messageStatus}'.
                        SDK branch: `${SDK_BRANCH}`
                        SDK commit: `${sdk_commit}`
                        MEGAchat branch: `${MEGACHAT_BRANCH}`
                        MEGAchat commit: `${megachat_commit}`
                    """.stripIndent()
                    withCredentials([string(credentialsId: 'slack_webhook_sdk_report', variable: 'SLACK_WEBHOOK_URL')]) {
                        sh """
                            curl -X POST -H 'Content-type: application/json' --data '
                                {
                                "attachments": [
                                    {
                                        "color": "${messageColor}",
                                        "blocks": [
                                        {
                                            "type": "section",
                                            "text": {
                                                    "type": "mrkdwn",
                                                    "text": "${message}"
                                            }
                                        }
                                        ]
                                    }
                                    ]
                                }' ${SLACK_WEBHOOK_URL}
                        """
                    }
                }
                deleteDir()
            }
        }
    }
}
// vim: syntax=groovy tabstop=4 shiftwidth=4
