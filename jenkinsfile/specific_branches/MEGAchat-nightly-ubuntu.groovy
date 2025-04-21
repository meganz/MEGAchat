pipeline {
    agent { label 'linux && docker' }
    options {
        timeout(time: 300, unit: 'MINUTES')
        buildDiscarder(logRotator(numToKeepStr: '135', daysToKeepStr: '21'))
        gitLabConnection('GitLabConnectionJenkins')
    }
    stages {
        stage('Checkout SDK and MEGAchat'){
            steps {
                deleteDir()
                checkout scm
                dir('third-party/mega'){
                    sh "echo Cloning SDK branch \"${SDK_BRANCH}\""
                    checkout([
                        $class: 'GitSCM',
                        branches: [[name: "${SDK_BRANCH}"]],
                        userRemoteConfigs: [[ url: "git@code.developers.mega.co.nz:sdk/sdk.git", credentialsId: "12492eb8-0278-4402-98f0-4412abfb65c1" ]],
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
                                    -v ${WORKSPACE}:/mega/megachat \
                                    megachat-linux-build:${IMAGE_TAG}
                            """
                        }
                    }
                }
                post {
                    always {
                        deleteDir()
                        sh "docker rmi -f megachat-linux-build:${IMAGE_TAG}"
                    }
                }
            }
        }
    }
}
// vim: syntax=groovy tabstop=4 shiftwidth=4
