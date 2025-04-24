pipeline {
    agent { label 'linux && docker' }
    options {
        timeout(time: 300, unit: 'MINUTES')
        buildDiscarder(logRotator(numToKeepStr: '135', daysToKeepStr: '21'))
        gitLabConnection('GitLabConnectionJenkins')
    }
    parameters {
        booleanParam(name: 'RESULT_TO_SLACK', defaultValue: true, description: 'Should the job result be sent to slack?')
        string(name: 'SDK_BRANCH', defaultValue: 'develop', description: 'Define a custom SDK branch.')
        string(name: 'MEGACHAT_BRANCH', defaultValue: 'develop', description: 'Define a custom MEGAchat branch.')
    }
    environment {
        SDK_BRANCH = "${params.SDK_BRANCH}"
        MEGACHAT_BRANCH = "${params.MEGACHAT_BRANCH}"
    }
    stages {
        stage('Checkout SDK and MEGAchat'){
            steps {
                deleteDir()
                checkout([
                    $class: 'GitSCM',
                    branches: [[name: "${MEGACHAT_BRANCH}"]],
                    userRemoteConfigs: [[ url: "${GIT_URL_MEGACHAT}", credentialsId: "12492eb8-0278-4402-98f0-4412abfb65c1" ]],
                    extensions: [
                        [$class: "UserIdentity",name: "jenkins", email: "jenkins@jenkins"]
                    ]
                ])
                dir('third-party/mega'){
                    sh "echo Cloning SDK branch \"${SDK_BRANCH}\""
                    checkout([
                        $class: 'GitSCM',
                        branches: [[name: "${SDK_BRANCH}"]],
                        userRemoteConfigs: [[ url: "${GIT_URL_SDK}", credentialsId: "12492eb8-0278-4402-98f0-4412abfb65c1" ]],
                        extensions: [
                            [$class: "UserIdentity",name: "jenkins", email: "jenkins@jenkins"]
                        ]
                    ])
                }
                script{
                    megachat_sources_workspace = WORKSPACE
                    sdk_sources_workspace = "${megachat_sources_workspace}/third-party/mega"
                }
            }
        }
        stage('Build'){
            matrix {
                axes {
                    axis {
                        name 'IMAGE_TAG'
                        values 'ubuntu-2204' , 'ubuntu-2404'
                    }
                }
                stages{
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
                                    -f dockerfile/linux-build.dockerfile \
                                    -t megachat-linux-build:${IMAGE_TAG} . \
                                    --no-cache --pull --progress=plain
                            """
                        }
                    }
                    stage("Build MEGAChat") {
                        steps {
                            sh """
                                 docker run \
                                    --rm \
                                    -v ${WORKSPACE}:/mega/megachat \
                                    megachat-linux-build:${IMAGE_TAG}
                            """
                        }
                    }
                }
                post {
                    always {
                        sh "docker rmi -f megachat-linux-build:${IMAGE_TAG}"
                    }
                }
            }
        }
    }
    post {
        always {
            script {
                if (params.RESULT_TO_SLACK) {
                    def megachat_commit = sh(script: "git -C ${megachat_sources_workspace} rev-parse HEAD", returnStdout: true).trim()
                    def sdk_commit = sh(script: "git -C ${sdk_sources_workspace} rev-parse HEAD", returnStdout: true).trim()
                    def messageStatus = currentBuild.currentResult
                    def messageColor = messageStatus == 'SUCCESS'? "#00FF00": "#FF0000" //green or red
                    message = """
                        *MEGAchat* nightly build <${BUILD_URL}|Build result>: '${messageStatus}'.
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
