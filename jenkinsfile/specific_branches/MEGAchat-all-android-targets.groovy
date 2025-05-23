def failedTargets = []

pipeline {
    agent { label 'linux && amd64 && docker' }
    options {
        timeout(time: 300, unit: 'MINUTES')
        buildDiscarder(logRotator(numToKeepStr: '135', daysToKeepStr: '21'))
        gitLabConnection('GitLabConnectionJenkins')
    }
    parameters {
        booleanParam(name: 'RESULT_TO_SLACK', defaultValue: true, description: 'Should the job result be sent to slack?')
        booleanParam(name: 'BUILD_ARM', defaultValue: true, description: 'Build for ARM')
        booleanParam(name: 'BUILD_ARM64', defaultValue: true, description: 'Build for ARM64')
        booleanParam(name: 'BUILD_X86', defaultValue: true, description: 'Build for X86')
        booleanParam(name: 'BUILD_X64', defaultValue: true, description: 'Build for X64')
        string(name: 'MEGACHAT_BRANCH', defaultValue: 'develop', description: 'Define a custom SDK branch.')
        string(name: 'SDK_BRANCH', defaultValue: 'develop', description: 'Define a custom SDK branch.')
    }
    environment {
        VCPKGPATH = "/opt/vcpkg"
        VCPKGPATH_CACHE = "${HOME}/.cache/vcpkg"
    }
    stages {
        stage('Checkout SDK and MEGAchat'){
            steps {
                deleteDir()
                sh "echo Cloning MEGAchat branch \"${params.MEGACHAT_BRANCH}\""
                checkout([
                    $class: 'GitSCM',
                    branches: [[name: "${params.MEGACHAT_BRANCH}"]],
                    userRemoteConfigs: [[ url: "git@code.developers.mega.co.nz:megachat/MEGAchat.git", credentialsId: "12492eb8-0278-4402-98f0-4412abfb65c1" ]],
                    extensions: [
                        [$class: "UserIdentity",name: "jenkins", email: "jenkins@jenkins"]
                    ]
                ])
                dir('third-party/mega'){
                    sh "echo Cloning SDK branch \"${params.SDK_BRANCH}\""
                    checkout([
                        $class: 'GitSCM',
                        branches: [[name: "${params.SDK_BRANCH}"]],
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
        stage('Build Android docker image'){
            steps{
                dir("dockerfile"){
                    sh "docker build -t meganz/megachat-android-build-env:${env.BUILD_NUMBER} -f ./android-cross-build.dockerfile ."
                }
            }
        }
        stage('Get UID and GID') {
            steps {
                script {
                    def uid = sh(script: 'id -u', returnStdout: true).trim()
                    def gid = sh(script: 'id -g', returnStdout: true).trim()
                    env.UID = uid
                    env.GID = gid
                }
            }
        }
        stage('Build with docker'){
            parallel {
                stage('Build arm'){
                    when {
                        beforeAgent true
                        expression { params.BUILD_ARM == true }
                    }
                    steps {
                        sh "docker run --name megachat-android-builder-arm-${env.BUILD_NUMBER} --rm -v ${WORKSPACE}:/mega/MEGAchat -v ${VCPKGPATH}:/mega/vcpkg -v ${VCPKGPATH_CACHE}:/mega/.cache/vcpkg -e ARCH=arm meganz/megachat-android-build-env:${env.BUILD_NUMBER}"
                        sh "docker run --name megachat-android-builder-arm-dynamiclib-${env.BUILD_NUMBER} --rm -v ${WORKSPACE}:/mega/MEGAchat -v ${VCPKGPATH}:/mega/vcpkg -v ${VCPKGPATH_CACHE}:/mega/.cache/vcpkg -e ARCH=arm -e BUILD_SHARED_LIBS=ON meganz/megachat-android-build-env:${env.BUILD_NUMBER}"
                    }
                    post{
                        aborted {
                            sh "docker kill megachat-android-builder-arm-${env.BUILD_NUMBER}; docker kill megachat-android-builder-arm-dynamiclib-${env.BUILD_NUMBER}"
                            script {
                                failedTargets.add("arm")
                            }
                        }
                        failure {
                            script {
                                failedTargets.add("arm")
                            }
                        }
                    }
                }
                stage('Build arm64'){
                    when {
                        beforeAgent true
                        expression { params.BUILD_ARM64 == true }
                    }
                    steps {
                        sh "docker run --name megachat-android-builder-arm64-${env.BUILD_NUMBER} --rm -v ${WORKSPACE}:/mega/MEGAchat -v ${VCPKGPATH}:/mega/vcpkg -v ${VCPKGPATH_CACHE}:/mega/.cache/vcpkg -e ARCH=arm64 meganz/megachat-android-build-env:${env.BUILD_NUMBER}"
                        sh "docker run --name megachat-android-builder-arm64-dynamiclib-${env.BUILD_NUMBER} --rm -v ${WORKSPACE}:/mega/MEGAchat -v ${VCPKGPATH}:/mega/vcpkg -v ${VCPKGPATH_CACHE}:/mega/.cache/vcpkg -e ARCH=arm64 -e BUILD_SHARED_LIBS=ON meganz/megachat-android-build-env:${env.BUILD_NUMBER}"
                    }
                    post{
                        aborted {
                            sh "docker kill megachat-android-builder-arm64-${env.BUILD_NUMBER}; docker kill megachat-android-builder-arm64-dynamiclib-${env.BUILD_NUMBER}" 
                            script {
                                failedTargets.add("arm64")
                            }
                        }
                        failure {
                            script {
                                failedTargets.add("arm64")
                            }
                        }
                    }
                }
                stage('Build x86'){
                    when {
                        beforeAgent true
                        expression { params.BUILD_X86 == true }
                    }
                    steps {
                        sh "docker run --name megachat-android-builder-x86-${env.BUILD_NUMBER} --rm -v ${WORKSPACE}:/mega/MEGAchat -v ${VCPKGPATH}:/mega/vcpkg -v ${VCPKGPATH_CACHE}:/mega/.cache/vcpkg -e ARCH=x86 meganz/megachat-android-build-env:${env.BUILD_NUMBER}"
                        sh "docker run --name megachat-android-builder-x86-dynamiclib-${env.BUILD_NUMBER} --rm -v ${WORKSPACE}:/mega/MEGAchat -v ${VCPKGPATH}:/mega/vcpkg -v ${VCPKGPATH_CACHE}:/mega/.cache/vcpkg -e ARCH=x86 -e BUILD_SHARED_LIBS=ON meganz/megachat-android-build-env:${env.BUILD_NUMBER}"
                     }
                    post{
                        aborted {
                            sh "docker kill megachat-android-builder-x86-${env.BUILD_NUMBER}; docker kill megachat-android-builder-x86-dynamiclib-${env.BUILD_NUMBER}" 
                            script {
                                failedTargets.add("x86")
                            }
                        }
                        failure {
                            script {
                                failedTargets.add("x86")
                            }
                        }
                    }
                }
                stage('Build x64'){
                    when {
                        beforeAgent true
                        expression { params.BUILD_X64 == true }
                    }
                    steps {
                        sh "docker run --name megachat-android-builder-x64-${env.BUILD_NUMBER} --rm -v ${WORKSPACE}:/mega/MEGAchat -v ${VCPKGPATH}:/mega/vcpkg -v ${VCPKGPATH_CACHE}:/mega/.cache/vcpkg -e ARCH=x64 meganz/megachat-android-build-env:${env.BUILD_NUMBER}"
                        sh "docker run --name megachat-android-builder-x64-dynamiclib-${env.BUILD_NUMBER} --rm -v ${WORKSPACE}:/mega/MEGAchat -v ${VCPKGPATH}:/mega/vcpkg -v ${VCPKGPATH_CACHE}:/mega/.cache/vcpkg -e ARCH=x64 -e BUILD_SHARED_LIBS=ON meganz/megachat-android-build-env:${env.BUILD_NUMBER}"
                    }
                    post{
                        aborted {
                            sh "docker kill megachat-android-builder-x64-${env.BUILD_NUMBER}; docker kill megachat-android-builder-x64-dynamiclib-${env.BUILD_NUMBER}" 
                            script {
                                failedTargets.add("x64")
                            }
                        }
                        failure {
                            script {
                                failedTargets.add("x64")
                            }
                        }
                    }
                }
            }
        }
    }
    post {
        always {
            sh "docker image rm meganz/megachat-android-build-env:${env.BUILD_NUMBER}"
            script {
                if (params.RESULT_TO_SLACK) {
                    def sdk_commit = sh(script: "git -C ${sdk_sources_workspace} rev-parse HEAD", returnStdout: true).trim()
                    def megachat_commit = sh(script: "git -C ${megachat_sources_workspace} rev-parse HEAD", returnStdout: true).trim()
                    def messageStatus = currentBuild.currentResult
                    def messageColor = messageStatus == 'SUCCESS'? "#00FF00": "#FF0000" //green or red
                    def message = """
                        *MEGAchat Android* <${BUILD_URL}|Build result>: '${messageStatus}'.
                        MEGAchat branch: `${MEGACHAT_BRANCH}`
                        MEGAchat commit: `${megachat_commit}`
                        SDK branch: `${SDK_BRANCH}`
                        SDK commit: `${sdk_commit}`
                    """.stripIndent()
                    
                    if (failedTargets.size() > 0) {
                        message += "\nFailed targets: ${failedTargets.join(', ')}"
                    }

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
                                }' \${SLACK_WEBHOOK_URL}
                        """
                    }
                }
            }
            deleteDir() /* clean up our workspace */
        }
    }
}
// vim: syntax=groovy tabstop=4 shiftwidth=4
